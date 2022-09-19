
# Table of Contents

1. [Introduction to Indigo](#1-introduction-to-indigo)
2. [Compiling and running the interpreter](#2-compiling-and-running-the-interpreter)
3. [Overview of syntax](#3-overview-of-syntax)
4. [Informal semantics](#4-informal-semantics)
    1. [Modules](#41-modules)
    2. [Constructors](#42-constructors)
    3. [Print statements](#43-print-statements)
    4. [Destructors](#44-destructors)
    5. [Recursion](#45-recursion)
    6. [Extensibility](#46-extensibility)
    7. [Dependent types](#47-dependent-types)
    8. [Static functions and delayed evaluation](#48-static-functions-and-delayed-evaluation)
    9. [Namespaces](#49-namespaces)
5. [Limitations and future directions](#5-limitations-and-future-directions)
6. [Additional remarks](#6-additional-remarks)

# 1. Introduction to Indigo

This repository contains code and examples for an experimental programming language I created a bit over a year ago. I knew relatively little about programming language semantics at the time, so I did not have the tools to analyze or prove any theoretical properties of the language. The language itself was inspired by my independent study of dependent type theory in the context of HoTT and proof assistants.

The name "indigo" predates this iteration of the programming language, and I have really just been using it as a working title for any programming language that I attempt to create. Originally, it was meant to be a combination of "inductive" and "go," with the idea being that I wanted to combine the minimalist philosophy of the go programming language with the ability to express inductive types and proofs. However, it is possible the programming language has diverged somewhat and will continue to diverge from this original motivation, and so I am open to suggestions for alternative names.

I implemented an interpreter for the language in C, whose source code is contained in the file `interpreter.c`. (Don't ask me why I chose C, for some reason I was just obsessed with it back then.) I would not recommend trying to read or understand the source code; it contains no comments, lots of code duplication, and way too many `goto` statements for error handling. The interpreter's error messages are not very descriptive, but they do at least specify at which character in the program the error was detected. (As a last-minute addition, I also made the program print out the names of all of the error `goto`s encountered when propagating an error up. However, you should take these with a grain of salt, as the names of these `goto`s were not originally designed with comprehensibility in mind.)

# 2. Compiling and running the interpreter

The source code for the interpreter is contained in a single C file and does not have any external dependencies, so if you are on OSX or Linux, you should (fingers crossed) be able to compile it by downloading `interpreter.c`, navigating to its enclosing folder in a command line, and running `gcc interpreter.c -o interpreter` (assuming you have GCC installed). I haven't tested the interpreter on Windows, and I suspect it will not compile on Windows as-is; if this is a problem, let me know and I can see about making the necessary code modifications.

Once you have compiled the interpreter, you can run it from the command line using the command `./interpreter` (or by writing the full path to the interpreter executable if it is not contained in the current working directory). Once you run the interpreter, it will search the current working directory for a file called `main.ind`, which will be treated as the entry point for the program. Programs are parsed in one pass from start to finish, and one the interpreter reaches the end of `main.ind` without encountering any syntax or typing errors, it will perform a final validation step to make sure that all necessary cases have been implemented. The `main.ind` file can include other files using the syntax `<file_path>`, which can be seen as essentially just copying the contents of `file_path` into `main.ind`; this can be done recursively, but it is important to note that all file paths are taken relative to the original working directly.

# 3. Overview of syntax

Indigo has three main kinds of syntactic structures: declarations, expressions, and evaluations. Broadly speaking, declarations are top-level commands that change that help define new types and functionality, whereas expressions and evaluations are used to build objects or invoke functions. The (more or less) precise syntax may be expressed as follows. Some remarks about the notation here:

* We write `⟪expression⟫*` to indicate that `expression` may be matched against an arbitrary nonnegative number of times, analogously to `(expression)*` in a regex.
* We write `⟦chars⟧+` to mean the same thing as the regular expression `[chars]+`.
* We separate different syntax forms by placing them on separate lines rather than using the `|` character so that we can reserve this character for the syntax itself.
* Unlike in regular expressions, we use `.` and `?` to denote literal characters.
* Extra whitespace is implicitly allowed everywhere except in `⟦chars⟧+` expressions.
* Names for most of the syntax forms are listed on the right in parentheses.

using regex-style syntax, where `⟪` and `⟫` are used in place of parentheses, `⟦` and `⟧` are used in place of brackets (so that literal parentheses and brackets do not need to be escaped), `.` is interpreted literally, and syntax forms are listed one per line with `|` standing for the literal character rather than delimiting different forms:

```
file names:
    f ::=
        ⟦^>⟧+

words:
    x, y, z, w ::=
        ⟦\w_+\-*\/%^&='"\\,`⟧+
symbols:
    s, t, u ::=
        ⟪x :⟫* y

expressions:
    a, b, c, A, B, C ::=
        s ⟪b⟫*                                   (constructions)
        (x ⟪d⟫*)                                 (evaluations)
        $A [a ⟪d⟫*]                              (annotations)
        ?                                        (construction question marks)
        (?)                                      (parameter question marks)
destructions:
    d ::=
        . t ⟪a⟫*                                 (ordinary destructions)
        . ?                                      (destruction question marks)

declarations:
    D ::=
        A ⟪(x)⟫* | s ⟪B [y]⟫*;                   (constructor declarations)
        A ⟪(x)⟫* . t ⟪B [y]⟫* ~ C;               (destructor declarations)
        A ⟪(x)⟫* [s ⟪(y)⟫* . t ⟪(z)⟫*] ~ c;      (rule declarations)
        @ u { ⟪D⟫* }                             (namespaces)
        $A [a ⟪d⟫*];                             (print statements)
        <f>                                      (file includes)
        #⟦^\n⟧*\n                                (line comments)

programs:
    P ::=
        ⟪D⟫*
```

# 4. Informal semantics

In this section I will attempt to give an intuition for how the language works internally, as well as how to use it to (in theory) create useful programs. All examples are numbered and implemented in the `examples` folder; to run one of them, simply replace the contents of `main.ind` with `<examples/XX.ind>`, where `XX` is the example number. We will use a comment of the form `# Example XX !?` to indicate that a given example results in an error.

## 4.1. Modules

When parsing an Indigo program, the interpreter reads the program one declaration at a time, from start to finish. As the interpreter parses declarations, it builds an internal representation of the program structure, which in the code is referred to as a **module**. A module contains a list of **matrices**, one for each type that has been declared. Finally, a matrix keeps track of all the **constructors**, **destructors**, and **rules** associated to each type, as they are declared. Before parsing begins, a module is initialized with a single primitive type `Type`, which has its own matrix of constructors and destructors.

## 4.2. Constructors

Intuitively, all types in Indigo are treated as algebraic data types: they have a finite list of constructors, each of which may take an arbitrary number of parameters of arbitrary types. The 'constructor declaration' syntax is used for adding new constructors to a type. For example, to define the type of booleans, we can use the following syntax:

```
# Example 4.2.1

Type|Bool;
Bool|true;
Bool|false;
```

Here, the first line adds the constructor `Bool` to the type `Type`, and the second and third lines add the constructors `true` and `false` to the type `Bool`.

Because constructors can take parameters, we can use this syntax to define inductive types as well. For instance, we can define the type of natural numbers:

```
# Example 4.2.2

Type|Nat;
Nat|zero;
Nat|succ Nat [n];
```

or the type of binary rooted trees:

```
# Example 4.2.3

Type|Tree;
Tree|leaf;
Tree|branch Tree [lhs] Tree [rhs];
```

It is also possible to define mutually inductive types; e.g.,

```
# Example 4.2.4

Type|Even;
Type|Odd;
Even|zero;
Even|succ Odd [n];
Odd|succ Even [n];
```

> Warning: Because programs are parsed in a single pass, any types that a statement references must have been declared ahead of time. Thus, for instance, the program

```
# Example 4.2.5 !?

Type|Even;
Even|zero;
Even|succ Odd [n];

Type|Odd;
Odd|succ Even [n];
```

> is not well-formed.

## 4.3. Print statements

In order to extract information from the module that the interpreter has constructed, we can use a **print statement**, which starts with a `$` symbol and tells the interpreter to evaluate a particular expression and print it to the command line. For instance, to make sure that the interpreter has actually internalized our definition for `Bool` from Example 4.2.1, we could write

```
# Example 4.3.1

Type|Bool;
Bool|true;
Bool|false;

$Bool [true];
```

Upon running the interpreter on this example, we will receive the output `true`. However, if we attempt to run a program such as

```
# Example 4.3.2 !?

Type|Bool;
Bool|true;
Bool|false;

$Bool [frue];
```

then we will get an error because the type `Bool` has no constructor names `frue`. In Indigo, all constructors and destructors must be accessed through their corresponding types. This allows us to, for instance, have multiple distinct types with overlapping constructor names, but has the downside of requiring us to annotate the types of expressions in many cases where it might feel unnecessary.

> Warning: The brackets in this example cannot be removed: the example

```
# Example 4.3.3 !?

Type|Bool;
Bool|true;
Bool|false;

$Bool true;
```

> produces an error. While it would be a straightforward change to disable these brackets, I included them because they help a lot with readability when dependent types are involved.

## 4.4. Destructors

To perform any meaningful computation, we must introduce **destructors**. Destructors in Indigo are analogous to methods in any object-oriented language, and they follow a similar syntax. As a first example, we can define negation on booleans as follows:

```
# Example 4.4.1

Type|Bool;
Bool|true;
Bool|false;

Bool.negate ~ Bool;            # (1)
Bool [true.negate] ~ false;    # (2)
Bool [false.negate] ~ true;    # (3)

$Bool [true.negate];           # prints 'false'
$Bool [false.negate];          # prints 'true'

# ...
```

Here, statement (1) declares a new destructor for the type `Bool` called `negate` that returns an object of type `Bool`, and statements (2) and (3) define rules for how `negate` operators on each of the constructors `true` and `false`.

It is also possible to have destructors with parameters; for instance we can define `and` and `or` operations as follows:

```
# Example 4.4.1 cont.

# ...

Bool.and Bool [x] ~ Bool;        # (6)
Bool [true.and (x)] ~ (x);       # (7)
Bool [false.and (x)] ~ false;    # (8)

Bool.or Bool [x] ~ Bool;         # (9)
Bool [true.or (x)] ~ true;       # (10)
Bool [false.or (x)] ~ (x);       # (11)

$Bool [true.or false];           # prints 'true'
$Bool [true.and false];          # prints 'false'

# ...
```

The types of the variables `x` does not need to be annotated in lines (7), (9), (10), and (11); the types are inferred from the declarations in (6) and (9).

> Warning: Note that in lines (7) and (11), the reference to the variable `x` after the `~` symbol (i.e., in the return expression) is wrapped in parentheses. In Indigo, words that refer to constructors must never be surrounded by parentheses, and words that refer to local variables must always be surrounded by parentheses. The reason I opted for this is a bit technical, and it is possible that improvements could be made to this syntax.

## 4.5. Recursion

It is possible to define recursive functions in Indigo in a similar way to inductive types. For instance, the following example defines natural numbers together with the operations of addition and multiplication.

```
# Example 4.5.1

Type|Nat;
Nat|zero;
Nat|succ Nat [n];

Nat.add Nat [x] ~ Nat;
Nat [zero.add (x)] ~ (x);
Nat [succ (n).add (x)] ~ succ (n.add (x));       # (1)

Nat.mul Nat [x] ~ Nat;
Nat [zero.mul (x)] ~ zero;
Nat [succ (n).mul (x)] ~ (n.mul (x).add (x));    # (2)

# Q: what is 2+2? A: 4.
$Nat [succ succ zero.add succ succ zero];        # † prints 'succ succ succ succ zero'

# Q: what is 2*2? A: 4.
$Nat [succ succ zero.mul succ succ zero];        # † prints 'succ succ succ succ zero'

```

(† If these expressions seem confusing, that's because they are; see [this remark](#parsability-and-readability).)

Note that in line (1), the destructor `add` is called within the same set of parentheses as `n`, and that in line (2), the destructors `mul (x)` and `add (x)` are called in sequence. In general, an **evaluation**, which is an expression contained in a set of parentheses, consists of a variable (the caller) together with a sequence of **destructions** (expressions starting with a `.` followed by a destructor name).

> Warning: Destructions are only allowed after variable references, not after constructors; for instance, the program

```
# Example 4.5.2 !?

Type|Nat;
Nat|zero;
Nat|succ Nat [n];

Nat.add Nat [x] ~ Nat;
Nat [zero.add (x)] ~ (x);
Nat [succ (n).add (x)] ~ succ (n.add (x));


Type|Void;
Void|void;
Void.add_zero Nat [x] ~ Nat;
Void [void.add_zero (x)] ~ zero.add (x);    # (3)
```

> produces an error. The reason for this is that the compiler has no way of knowing whether `zero` in (3) should refer to a constructor for `Nat` or a constructor for some other type. To get around this, we can add an explicit **type annotation**:

```
# Example 4.5.3

# ...

Void [void.add_zero (x)] ~ $Nat [zero.add (x)];    # (4)
```

> Because of how computation works in Indigo, expressions that require type annotation correspond (more or less) exactly with expressions that can be reduced ahead of time, and this is exactly what happens internally. In the case of line (4), the interpreter will look up the constructor-destructor pair `zero.add` and reduce the expression to the right of `~` with `(x)`. If the case `zero.add` had not been specified yet, the example would produce an error.

## 4.6. Extensibility

Apart from the requirement that names not be referenced before they are declared, Indigo does not place restrictions on the order in which constructors, destructors, and rules may be defined. Indeed, this allows one to encode abstract data types in Indigo relatively straightforwardly. For example, the following program is perfectly valid:

```
# Example 4.6.1

# Declare abstract types for sounds and animals.

Type|Sound;

Type|Animal;
Animal.make_sound ~ Sound;

# Introduce cats.

Sound|meow;

Animal|cat;
Animal [cat.make_sound] ~ meow;

# Introduce dogs.

Sound|woof;

Animal|dog;
Animal [dog.make_sound] ~ woof;

# Tests.

$Animal [cat.make_sound];    # prints 'meow'
$Animal [dog.make_sound];    # prints 'woof'
```

We can also go a step further and make cats themselves abstract:

```
# Example 4.6.2

# Declare abstract types for sounds, names, and animals.

Type|Sound;

Type|Name;

Type|Animal;
Animal.make_sound ~ Sound;
Animal.name ~ Name;

# Introduce cats.

Sound|meow;

Type|Cat;
Cat.name ~ Name;

Animal|the_cat Cat [cat];
Animal [the_cat (cat).make_sound] ~ meow;
Animal [the_cat (cat).name] ~ (cat.name);

# Introduce Patrick and Penelope.

Name|patrick;
Name|penelope;

Cat|cat_1;
Cat [cat_1.name] ~ patrick;

Cat|cat_2;
Cat [cat_2.name] ~ penelope;

# Tests.

$Animal [the_cat cat_1.name];    # prints 'patrick'
$Animal [the_cat cat_2.name];    # prints 'penelope'

```

It is also possible to further intermingle the definitions of constructors and destructors for a type in Indigo. However, I suspect that in practice, the most useful cases will be ones where either all the constructors are defined up front, or all the destructors are. If you have an example in mind where you think it would be useful to intermingle constructors and destructors in some more complicated way, please let me know!

> Comparison with FamFun: I think Indigo and FamFun actually share a lot of things in common when it comes to how they implement extensibility. From what I can tell, Indigo is kind of like a version of FamFun where there is a single universal family that can be continually added onto. Both languages also deal with types "intensionally" (they distinguish types with the same structure but different names), and both languages allow/force the user to extract and name "match" expressions ("cases" statements in the case of FamFun and "destructors" in the case of Indigo) so that they can be extended by other parts of a program. I'd be excited to see if anyone (particularly Ana) notices any further similarities, or any useful features that exist in FamFun but not in Indigo.

## 4.7. Dependent types

In Indigo, dependent types (as well as type constructors, which are treated in the same way) can be declared in much the same way as ordinary constructors with arguments. For instance, the following example defines a type of lists of elements of a particular type;

```
# Example 4.7.1

Type|List Type [X];
List (X)|nil;
List (X)|cons (X) [x] List (X) [v];

Type|Bool;
Bool|true;
Bool|false;

$List Bool [cons true cons false nil];    # †
```
(† See [this remark](#parsability-and-readability))

The main new concept that this example introduces is that *constructors and destructors can refer to type arguments*.

In some cases, one has to be a little more clever in order to implement a dependent type correctly. The following is an example of how to implement a type of arrays of fixed length:

```
# Example 4.7.2

Type|Nat;
Nat|zero;
Nat|succ Nat [n];

# In addition to 'Nat', define a singleton type 'Void'
Type|Void;
Void|void;

# Also define a type of pairs consisting of an element of type 'X' and an element of type 'Y'.
Type|Pair Type [X] Type [Y];
Pair (X) (Y)|pair (X) [x] (Y) [y];

# Treat "the type of arrays consisting of 'n' elements of type 'X'" as a destructor operating on 'n'
Nat.Array Type [X] ~ Type;
Nat [zero.Array (X)] ~ Void;
Nat [succ (n).Array (X)] ~ Pair (X) (n.Array (X));

$Nat [succ succ zero.Array Nat];    # † prints 'Pair Nat Pair Nat Void'
```
(† See [this remark](#parsability-and-readability))

## 4.8. Static functions and delayed evaluation

In its current state, Indigo does not have any way of defining static functions; i.e., functions that can be invoked without reference to any particular object. However, there is a (admittedly pretty clunky) workaround: define a special type with a single element, then define all static functions as methods operating on that type. Later on, one can then refer to these functions using the annotation syntax. For example:

```
# Example 4.8.1

# We will treat all static functions as methods on 'O'.

Type|O;
O|*;

# Define natural numbers, together with addition.

Type|Nat;
Nat|zero;
Nat|succ Nat [n];

Nat.add Nat [x] ~ Nat;
Nat [zero.add (x)] ~ (x);
Nat [succ (n).add (x)] ~ succ (n.add (x));

# Define aliases for one and addition as methods on 'O'.

O.one ~ Nat;
O[*.one] ~ succ zero;

O.add Nat [x] Nat [y] ~ Nat;
O[*.add (x) (y)] ~ (x.add (y));

$O[*.one];                                               # prints 'succ zero'
$O[*.add $O[*.one] $O[*.one]];                           # prints 'succ succ zero'

O.add_2 Nat [x] Nat [y] ~ Nat;
O [*.add_2 (x) (y)] ~ $O [*.add (x) (y)];                # (1) unrolls to '(x.add (y))'
O.add_delayed O[o] Nat [x] Nat [y] ~ Nat;
O [*.add_delayed (o) (x) (y)] ~ (o.add (x) (y));    # (2) does not unroll

$O[*.add_delayed * $O[*.one] $O[*.one]];                 # (3) prints 'succ succ zero'
```

By default, all such static functions will be "unrolled" as far as possible whenever they are referenced in an expression; for instance, this is what happens in line (1). If desired, one can avoid this by adding extra parameters to methods, such as in lines (2) and (3).

## 4.9. Namespaces

For larger programs, it is possible to wrap certain constructors and destructors inside namespaces. The basic syntax of this is demonstrated below:

```
# Example 4.9.1

Type|Bool1;

@foo {
    Bool1|true;
    Bool1|false;
    
    Bool1.negate ~ Bool1;
    Bool1 [true.negate] ~ false;
    Bool1 [false.negate] ~ true;

    Type|Bool2;
    Bool2|true;
    Bool2|false;
    
    Bool2.negate ~ Bool2;
    
    Bool2 [true.negate] ~ false;
    Bool2 [false.negate] ~ true;
}

$Bool1 [foo:true.foo:negate];    # (1) prints 'foo:false'
$foo:Bool2 [true.negate];        # (2) prints 'false'
```

In particlar, note that if a type is defined outside a namespace, then constructors and destructors for that type defined within the namespace must be prefixed, whereas if the type is also defined inside the namespace, only the type itself needs to be prefixed.

> Warning: It is also possible in Indigo to continue to add to a namespace even after its initial declaration has ended; for instance, the following program is well-formed:

```
# Example 4.9.2

@foo {
    Type|A;
    A|a;
}

@foo {
    Type|B;
    B|b foo:A [x];    # (3)
}

$foo:A [a];           # prints 'a'
$foo:B [b a];         # prints 'b a'
```

> However, note that once the interpreter exits the first iteration of the namespace block, all subsequent references, *even those inside the second iteration of the `foo` namespace* (e.g., line (3)), must prefix any constructors or destructors from `foo`.

# 5. Limitations and future directions

> (If possible, I will try to fill this section in at some point before Thursday.)

# 6. Additional remarks

## Parsability and readability

In Indigo, expressions that do not refer to any variables never require (indeed are never allowed to have) parentheses because the interpreter keeps track of how many of each kind of expression it expects as it goes. For simple examples, this can be kind of convenient, but for more complicated ones, it can mean that expressions are difficult to parse manually.
