
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#define throw(error) do { \
    fprintf(stderr, #error ":\n"); \
    goto error; \
} while (false)

char const* MAIN_FILE_NAME = "main.ind";

typedef struct Parser {
    FILE* pFile;
    size_t lineNumber;
    size_t columnNumber;
    int next;
} Parser;
bool createParserFromFile(char const* pFileName, Parser* pParser);
void destroyParser(Parser parser);
void parser_advance(Parser* pParser);
void parser_skipWhitespace(Parser* pParser);

typedef struct String {
    size_t length;
    char* pData;
} String;
bool createStringFromCString(char const* pCString, String* pString);
void destroyString(String string);
bool string_equals(String string, String other);
bool string_print(String string);
bool parser_parseWord(Parser* pParser, String* pWord);
bool parser_parseName(Parser* pParser, String* pName);
bool parser_parseFileName(Parser* pParser, String* pFileName);

typedef enum ExpressionKind {
    UNSPECIFIED_EXPRESSION,
    CONSTRUCTION_EXPRESSION,
    EVALUATION_EXPRESSION
} ExpressionKind;
typedef struct Expression {
    ExpressionKind kind;
    void* pData;
} Expression;
typedef struct Construction {
    size_t index;
    size_t argumentCount;
    Expression* pArguments;
} Construction;
typedef enum EvaluationKind {
    REFERENCE_EVALUATION,
    DESTRUCTION_EVALUATION
} EvaluationKind;
typedef struct Evaluation {
    EvaluationKind kind;
    void* pData;
} Evaluation;
typedef struct Destruction {
    Evaluation caller;
    size_t index;
    size_t argumentCount;
    Expression* pArguments;
} Destruction;
bool createConstructionExpression(Construction construction, Expression* pExpression);
bool createEvaluationExpression(Evaluation evaluation, Expression* pExpression);
void destroyExpression(Expression expression);
bool createReferenceEvaluation(size_t index, Evaluation* pEvaluation);
bool createDestructionEvaluation(Destruction destruction, Evaluation* pEvaluation);
void destroyEvaluation(Evaluation evaluation);
bool expression_equals(Expression expression, Expression other);
bool evaluation_equals(Evaluation evaluation, Evaluation other);
bool expression_duplicate(Expression expression, Expression* pResult);
bool evaluation_duplicate(Evaluation evaluation, Evaluation* pResult);

typedef struct Constructor {
    size_t depth;
    String name;
    size_t parameterCount;
    Expression* pParameterTypes;
} Constructor;
typedef struct Destructor {
    size_t depth;
    String name;
    size_t parameterCount;
    Expression* pParameterTypes;
    Expression returnType;
    Expression* pRules;
} Destructor;
typedef struct Matrix {
    size_t constructorCount;
    Constructor* pConstructors;
    size_t destructorCount;
    Destructor* pDestructors;
} Matrix;
typedef struct Module {
    size_t matrixCount;
    Matrix* pMatrices;
} Module;
typedef struct Parameter {
    String name;
    Expression type;
} Parameter;
typedef struct Substitution {
    Expression type;
    Expression value;
} Substitution;
bool createEmptyModule(Module* pModule);
void destroyModule(Module module);
bool expression_substitute(
    Expression expression, Module module, Substitution const* pSubstitutions,
    Expression* pResult
);
bool expression_print(
    Expression expression, Module module, size_t parameterCount, Parameter const* pParameters, Expression type
);
bool type_print(
    Expression expression, Module module, size_t parameterCount, Parameter const* pParameters
);
bool evaluation_print(
    Evaluation evaluation, Module module, size_t parameterCount, Parameter const* pParameters, Expression* pType
);
bool evaluation_substitute(
    Evaluation evaluation, Module module, Substitution const* pSubstitutions,
    Substitution* pResult
);
bool substitution_destruct(
    Substitution substitution, Module module, size_t index, Expression const* pArguments,
    Substitution* pResult
);
bool parser_parseExpression(
    Parser* pParser, Module module,
    size_t parameterCount, Parameter const* pParameters, Expression type,
    Expression* pExpression
);
bool parser_parseType(
    Parser* pParser, Module module,
    size_t parameterCount, Parameter const* pParameters,
    Expression* pExpression
);
bool parser_parseStatement(Parser* pParser, Module* pModule, size_t depth);
bool module_endNamespace(Module* pModule, size_t depth, char const* pNamespace);
bool module_validate(Module module, size_t depth);

bool parseFile(char const* pFileName, Module* pModule, size_t depth);



int main() {
    Module module;
    if (!createEmptyModule(&module))
        goto moduleCreateError;
    if (!parseFile(MAIN_FILE_NAME, &module, 0))
        goto fileParseError;
    if (!module_validate(module, 0))
        goto moduleValidateError;
    destroyModule(module);
    return EXIT_SUCCESS;
    
moduleValidateError:
fileParseError:
    destroyModule(module);
moduleCreateError:
    return EXIT_FAILURE;
}



bool createParserFromFile(char const* pFileName, Parser* pParser) {
    FILE* pFile = fopen(pFileName, "r");
    if (pFile == NULL)
        throw(fileOpenError);
    int next = fgetc(pFile);
    
    *pParser = (Parser) {
        .pFile = pFile,
        .lineNumber = 1,
        .columnNumber = 1,
        .next = next
    };
    return true;
    
    fclose(pFile);
fileOpenError:
    return false;
}
void destroyParser(Parser parser) {
    fclose(parser.pFile);
}
void parser_advance(Parser* pParser) {
    if (pParser->next == '\n') {
        pParser->lineNumber++;
        pParser->columnNumber = 1;
    } else
        pParser->columnNumber++;
    pParser->next = fgetc(pParser->pFile);
}
void parser_skipWhitespace(Parser* pParser) {
    while (isspace(pParser->next))
        parser_advance(pParser);
}

bool createStringFromCString(char const* pCString, String* pString) {
    size_t length = strlen(pCString);
    char* pData = malloc(length);
    if (pData == NULL)
        throw(dataMallocError);
    memcpy(pData, pCString, length);
    
    *pString = (String) {
        .length = length,
        .pData = pData
    };
    return true;
    
    free(pData);
dataMallocError:
    return false;
}
void destroyString(String string) {
    free(string.pData);
}
bool string_equals(String string, String other) {
    return string.length == other.length && memcmp(string.pData, other.pData, string.length) == 0;
}
bool string_print(String string) {
    for (size_t i = 0; i < string.length; i++) {
        if (fputc(string.pData[i], stdout) == EOF)
            return false;
    }
    return true;
}
bool parser_parseWord(Parser* pParser, String* pWord) {
    size_t length = 0;
    char* pData = malloc(1);
    if (pData == NULL)
        throw(dataReallocError);
    while (
        isalnum(pParser->next) ||
        pParser->next == '_' ||
        pParser->next == '+' ||
        pParser->next == '-' ||
        pParser->next == '*' ||
        pParser->next == '/' ||
        pParser->next == '%' ||
        pParser->next == '^' ||
        pParser->next == '&' ||
        pParser->next == '=' ||
        pParser->next == '\'' ||
        pParser->next == '"' ||
        pParser->next == '\\' ||
        pParser->next == ',' ||
        pParser->next == '`'
    ) {
        char* pNewData = realloc(pData, length + 2);
        if (pNewData == NULL)
            throw(dataReallocError);
        pData = pNewData;
        pData[length] = (char) pParser->next;
        length++;
        parser_advance(pParser);
    }
    pData[length] = 0;
    parser_skipWhitespace(pParser);
    *pWord = (String) {
        .length = length,
        .pData = pData
    };
    return true;
    
dataReallocError:
    free(pData);
    return false;
}
bool parser_parseName(Parser* pParser, String* pName) {
    size_t length = 0;
    char* pData = malloc(1);
    if (pData == NULL)
        throw(dataReallocError);
    do {
        while (
            isalnum(pParser->next) ||
            pParser->next == '_' ||
            pParser->next == '+' ||
            pParser->next == '-' ||
            pParser->next == '*' ||
            pParser->next == '/' ||
            pParser->next == '%' ||
            pParser->next == '^' ||
            pParser->next == '&' ||
            pParser->next == '=' ||
            pParser->next == '\'' ||
            pParser->next == '"' ||
            pParser->next == '\\' ||
            pParser->next == ',' ||
            pParser->next == '`'
        ) {
            char* pNewData = realloc(pData, length + 2);
            if (pNewData == NULL)
                throw(dataReallocError);
            pData = pNewData;
            pData[length] = (char) pParser->next;
            length++;
            parser_advance(pParser);
        }
        pData[length] = 0;
        parser_skipWhitespace(pParser);
        if (pParser->next != ':')
            break;
        char* pNewData = realloc(pData, length + 1);
        if (pNewData == NULL)
            throw(dataReallocError);
        pData = pNewData;
        pData[length] = (char) pParser->next;
        length++;
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
    } while (true);
    *pName = (String) {
        .length = length,
        .pData = pData
    };
    return true;

dataReallocError:
    free(pData);
    return false;
}
bool parser_parseFileName(Parser* pParser, String* pFileName) {
    size_t length = 0;
    char* pData = malloc(1);
    while (pParser->next != '<' && pParser->next != '>') {
        char* pNewData = realloc(pData, length + 2);
        if (pNewData == NULL)
            throw(dataReallocError);
        pData = pNewData;
        pData[length] = (char) pParser->next;
        length++;
        parser_advance(pParser);
    }
    pData[length] = 0;
    *pFileName = (String) {
        .length = length,
        .pData = pData
    };
    return true;

dataReallocError:
    free(pData);
    return false;
}

bool createConstructionExpression(Construction construction, Expression* pExpression) {
    Construction* pData = malloc(sizeof(Construction));
    if (pData == NULL)
        throw(dataMallocError);
    *pData = construction;
    
    *pExpression = (Expression) {
        .kind = CONSTRUCTION_EXPRESSION,
        .pData = pData
    };
    return true;
    
    free(pData);
dataMallocError:
    return false;
}
bool createEvaluationExpression(Evaluation evaluation, Expression* pExpression) {
    Evaluation* pData = malloc(sizeof(Evaluation));
    if (pData == NULL)
        throw(dataMallocError);
    *pData = evaluation;
    
    *pExpression = (Expression) {
        .kind = EVALUATION_EXPRESSION,
        .pData = pData
    };
    return true;
    
    free(pData);
dataMallocError:
    return false;
}
void destroyExpression(Expression expression) {
    if (expression.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pConstruction = expression.pData;
        for (size_t i = 0; i < pConstruction->argumentCount; i++)
            destroyExpression(pConstruction->pArguments[i]);
        free(pConstruction->pArguments);
    }
    if (expression.kind == EVALUATION_EXPRESSION) {
        Evaluation* pEvaluation = expression.pData;
        destroyEvaluation(*pEvaluation);
    }
    free(expression.pData);
}
bool createReferenceEvaluation(size_t index, Evaluation* pEvaluation) {
    size_t* pData = malloc(sizeof(size_t));
    if (pData == NULL)
        throw(dataMallocError);
    *pData = index;
    
    *pEvaluation = (Evaluation) {
        .kind = REFERENCE_EVALUATION,
        .pData = pData
    };
    return true;
    
    free(pData);
dataMallocError:
    return false;
}
bool createDestructionEvaluation(Destruction destruction, Evaluation* pEvaluation) {
    Destruction* pData = malloc(sizeof(Destruction));
    if (pData == NULL)
        throw(dataMallocError);
    *pData = destruction;
    
    *pEvaluation = (Evaluation) {
        .kind = DESTRUCTION_EVALUATION,
        .pData = pData
    };
    return true;
    
    free(pData);
dataMallocError:
    return false;
}
void destroyEvaluation(Evaluation evaluation) {
    if (evaluation.kind == DESTRUCTION_EVALUATION) {
        Destruction* pDestruction = evaluation.pData;
        for (size_t i = 0; i < pDestruction->argumentCount; i++)
            destroyExpression(pDestruction->pArguments[i]);
        free(pDestruction->pArguments);
        destroyEvaluation(pDestruction->caller);
    }
    free(evaluation.pData);
}
bool expression_equals(Expression expression, Expression other) {
    if (expression.kind != other.kind)
        return false;
    if (expression.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pConstruction = expression.pData;
        Construction* pOther = other.pData;
        if (pConstruction->index != pOther->index)
            return false;
        for (size_t i = 0; i < pConstruction->argumentCount; i++) {
            if (!expression_equals(pConstruction->pArguments[i], pOther->pArguments[i]))
                return false;
        }
        return true;
    }
    if (expression.kind == EVALUATION_EXPRESSION) {
        Evaluation* pEvaluation = expression.pData;
        Evaluation* pOther = other.pData;
        if (!evaluation_equals(*pEvaluation, *pOther))
            return false;
        return true;
    }
    return false;
}
bool evaluation_equals(Evaluation evaluation, Evaluation other) {
    if (evaluation.kind != other.kind)
        return false;
    if (evaluation.kind == REFERENCE_EVALUATION) {
        size_t* pReference = evaluation.pData;
        size_t* pOther = other.pData;
        if (*pReference != *pOther)
            return false;
        return true;
    }
    if (evaluation.kind == DESTRUCTION_EVALUATION) {
        Destruction* pDestruction = evaluation.pData;
        Destruction* pOther = other.pData;
        if (!evaluation_equals(pDestruction->caller, pOther->caller))
            return false;
        if (pDestruction->index != pOther->index)
            return false;
        for (size_t i = 0; i < pDestruction->argumentCount; i++) {
            if (!expression_equals(pDestruction->pArguments[i], pOther->pArguments[i]))
                return false;
        }
        return true;
    }
    return false;
}
bool expression_duplicate(Expression expression, Expression* pResult) {
    if (expression.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pData = expression.pData;
        
        Expression* pArguments = malloc(pData->argumentCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(constructionArgumentsMallocError);
        size_t argumentCount;
        for (argumentCount = 0; argumentCount < pData->argumentCount; argumentCount++) {
            if (!expression_duplicate(pData->pArguments[argumentCount], &pArguments[argumentCount]))
                throw(constructionArgumentDuplicateError);
        }
        
        Construction construction = {
            .index = pData->index,
            .argumentCount = argumentCount,
            .pArguments = pArguments
        };
        Expression result;
        if (!createConstructionExpression(construction, &result))
            throw(constructionExpressionCreateError);
        
        *pResult = result;
        return true;
    
        destroyExpression(result);
    constructionExpressionCreateError:
    constructionArgumentDuplicateError:
        for (size_t i = 0; i < argumentCount; i++)
            destroyExpression(pArguments[i]);
        free(pArguments);
    constructionArgumentsMallocError:
        return false;
    }
    if (expression.kind == EVALUATION_EXPRESSION) {
        Evaluation* pData = expression.pData;
        
        Evaluation evaluation;
        if (!evaluation_duplicate(*pData, &evaluation))
            throw(evaluationDuplicateError);
        
        Expression result;
        if (!createEvaluationExpression(evaluation, &result))
            throw(evaluationExpressionCreateError);
    
        *pResult = result;
        return true;
    
        destroyExpression(result);
    evaluationExpressionCreateError:
        destroyEvaluation(evaluation);
    evaluationDuplicateError:
        return false;
    }
    return false;
}
bool evaluation_duplicate(Evaluation evaluation, Evaluation* pResult) {
    if (evaluation.kind == REFERENCE_EVALUATION) {
        size_t* pData = evaluation.pData;
        
        Evaluation result;
        if (!createReferenceEvaluation(*pData, &result))
            throw(referenceEvaluationCreateError);
        
        *pResult = result;
        return true;
    
        destroyEvaluation(result);
    referenceEvaluationCreateError:
        return false;
    }
    if (evaluation.kind == DESTRUCTION_EVALUATION) {
        Destruction* pData = evaluation.pData;
        
        Evaluation caller;
        if (!evaluation_duplicate(pData->caller, &caller))
            throw(destructionCallerDuplicateError);
    
        Expression* pArguments = malloc(pData->argumentCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(destructionArgumentsMallocError);
        size_t argumentCount;
        for (argumentCount = 0; argumentCount < pData->argumentCount; argumentCount++) {
            if (!expression_duplicate(pData->pArguments[argumentCount], &pArguments[argumentCount]))
                throw(destructionArgumentDuplicateError);
        }
        
        Destruction destruction = {
            .index = pData->index,
            .caller = caller,
            .argumentCount = argumentCount,
            .pArguments = pArguments
        };
        Evaluation result;
        if (!createDestructionEvaluation(destruction, &result))
            throw(destructionEvaluationCreateError);
        
        *pResult = result;
        return true;
    
        destroyEvaluation(result);
    destructionEvaluationCreateError:
    destructionArgumentDuplicateError:
        for (size_t i = 0; i < argumentCount; i++)
            destroyExpression(pArguments[i]);
        free(pArguments);
    destructionArgumentsMallocError:
        destroyEvaluation(caller);
    destructionCallerDuplicateError:
        return false;
    }
    return false;
}

bool createEmptyModule(Module* pModule) {
    size_t matrixCount = 1;
    Matrix* pMatrices = malloc(sizeof(Matrix));
    if (pMatrices == NULL)
        throw(matricesMallocError);
    
    size_t typeConstructorCount = 1;
    Constructor* pTypeConstructors = malloc(sizeof(Constructor));
    if (pTypeConstructors == NULL)
        throw(typeConstructorsMallocError);
    
    String universeTypeName;
    if (!createStringFromCString("Type", &universeTypeName))
        throw(universeTypeNameCreateError);
    
    pTypeConstructors[0] = (Constructor) {
        .depth = 0,
        .name = universeTypeName,
        .parameterCount = 0,
        .pParameterTypes = NULL
    };
    pMatrices[0] = (Matrix) {
        .constructorCount = typeConstructorCount,
        .pConstructors = pTypeConstructors,
        .destructorCount = 0,
        .pDestructors = NULL
    };
    *pModule = (Module) {
        .matrixCount = matrixCount,
        .pMatrices = pMatrices
    };
    return true;
    
    destroyString(universeTypeName);
universeTypeNameCreateError:
    free(pTypeConstructors);
typeConstructorsMallocError:
    free(pMatrices);
matricesMallocError:
    return false;
}
void destroyModule(Module module) {
    for (size_t i = 0; i < module.matrixCount; i++) {
        Matrix matrix = module.pMatrices[i];
        for (size_t j = 0; j < matrix.destructorCount; j++) {
            Destructor destructor = matrix.pDestructors[j];
            for (size_t k = 0; k < matrix.constructorCount; k++)
                destroyExpression(destructor.pRules[k]);
            free(destructor.pRules);
            destroyExpression(destructor.returnType);
            for (size_t k = 0; k < destructor.parameterCount; k++)
                destroyExpression(destructor.pParameterTypes[k]);
            free(destructor.pParameterTypes);
            destroyString(destructor.name);
        }
        free(matrix.pDestructors);
        for (size_t j = 0; j < matrix.constructorCount; j++) {
            Constructor constructor = matrix.pConstructors[j];
            for (size_t k = 0; k < constructor.parameterCount; k++)
                destroyExpression(constructor.pParameterTypes[k]);
            free(constructor.pParameterTypes);
            destroyString(constructor.name);
        }
        free(matrix.pConstructors);
    }
    free(module.pMatrices);
}
bool expression_substitute(
    Expression expression, Module module, Substitution const* pSubstitutions,
    Expression* pResult
) {
    if (expression.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pData = expression.pData;
        
        Expression* pArguments = malloc(pData->argumentCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(constructionArgumentsMallocError);
        size_t argumentCount;
        for (argumentCount = 0; argumentCount < pData->argumentCount; argumentCount++) {
            if (!expression_substitute(
                pData->pArguments[argumentCount], module, pSubstitutions,
                &pArguments[argumentCount]
            ))
                throw(constructionArgumentSubstituteError);
        }
        
        Construction construction = {
            .index = pData->index,
            .argumentCount = argumentCount,
            .pArguments = pArguments
        };
        Expression result;
        if (!createConstructionExpression(construction, &result))
            throw(constructionExpressionCreateError);
        
        *pResult = result;
        return true;
    
        destroyExpression(result);
    constructionExpressionCreateError:
    constructionArgumentSubstituteError:
        for (size_t i = 0; i < argumentCount; i++)
            destroyExpression(pArguments[i]);
        free(pArguments);
    constructionArgumentsMallocError:
        return false;
    }
    if (expression.kind == EVALUATION_EXPRESSION) {
        Evaluation* pData = expression.pData;
        
        Substitution substitution;
        if (!evaluation_substitute(*pData, module, pSubstitutions, &substitution))
            throw(evaluationSubstituteError);
        
        *pResult = substitution.value;
        destroyExpression(substitution.type);
        return true;
    
        destroyExpression(substitution.value);
        destroyExpression(substitution.type);
    evaluationSubstituteError:
        return false;
    }
    return false;
}
bool evaluation_substitute(
    Evaluation evaluation, Module module, Substitution const* pSubstitutions,
    Substitution* pResult
) {
    if (evaluation.kind == REFERENCE_EVALUATION) {
        size_t* pData = evaluation.pData;
        
        Expression type;
        if (!expression_duplicate(pSubstitutions[*pData].type, &type))
            throw(referenceTypeDuplicateError);
        Expression value;
        if (!expression_duplicate(pSubstitutions[*pData].value, &value))
            throw(referenceValueDuplicateError);
        
        *pResult = (Substitution) {
            .type = type,
            .value = value
        };
        return true;
    
        destroyExpression(value);
    referenceValueDuplicateError:
        destroyExpression(type);
    referenceTypeDuplicateError:
        return false;
    }
    if (evaluation.kind == DESTRUCTION_EVALUATION) {
        Destruction* pData = evaluation.pData;
        
        Substitution caller;
        if (!evaluation_substitute(pData->caller, module, pSubstitutions, &caller))
            throw(destructionCallerSubstituteError);
    
        Expression* pArguments = malloc(pData->argumentCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(destructionArgumentsMallocError);
        size_t argumentCount;
        for (argumentCount = 0; argumentCount < pData->argumentCount; argumentCount++) {
            if (!expression_substitute(
                pData->pArguments[argumentCount], module, pSubstitutions,
                &pArguments[argumentCount]
            ))
                throw(destructionArgumentSubstituteError);
        }
        
        Substitution result;
        if (!substitution_destruct(caller, module, pData->index, pArguments, &result))
            throw(destructionDestructError);
    
        *pResult = result;
        for (size_t i = 0; i < argumentCount; i++)
            destroyExpression(pArguments[i]);
        free(pArguments);
        destroyExpression(caller.value);
        destroyExpression(caller.type);
        return true;
    
    destructionDestructError:
    destructionArgumentSubstituteError:
        for (size_t i = 0; i < argumentCount; i++)
            destroyExpression(pArguments[i]);
        free(pArguments);
    destructionArgumentsMallocError:
        destroyExpression(caller.value);
        destroyExpression(caller.type);
    destructionCallerSubstituteError:
        return false;
    }
    return false;
}
bool expression_print(
    Expression expression, Module module, size_t parameterCount, Parameter const* pParameters, Expression type
) {
    if (expression.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pData = expression.pData;
        if (type.kind != CONSTRUCTION_EXPRESSION)
            throw(constructionTypeError);
        Construction* pTypeConstruction = type.pData;
        Constructor typeConstructor = module.pMatrices[0].pConstructors[pTypeConstruction->index];
        Matrix matrix = module.pMatrices[pTypeConstruction->index];
    
        Constructor constructor = matrix.pConstructors[pData->index];
        if (!string_print(constructor.name))
            throw(constructionNamePrintError);
    
        Substitution* pSubstitutions = malloc(
            (typeConstructor.parameterCount + constructor.parameterCount) * sizeof(Substitution)
        );
        if (pSubstitutions == NULL)
            throw(constructionSubstitutionsMallocError);
        size_t typeSubstitutionCount;
        for (
            typeSubstitutionCount = 0;
            typeSubstitutionCount < typeConstructor.parameterCount;
            typeSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                typeConstructor.pParameterTypes[typeSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(constructionParameterTypeSubstituteError);
        
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pTypeConstruction->pArguments[typeSubstitutionCount]
            };
            continue;
        
            destroyExpression(parameterType);
        constructionParameterTypeSubstituteError:
            throw(constructionTypeSubstitutionsError);
        }
        size_t constructorSubstitutionCount;
        for (
            constructorSubstitutionCount = 0;
            constructorSubstitutionCount < constructor.parameterCount;
            constructorSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                constructor.pParameterTypes[constructorSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(constructionParameterConstructorSubstituteError);
            
            if (fputc(' ', stdout) == EOF)
                throw(constructionParameterConstructorArgumentPrintError);
            if (!expression_print(
                pData->pArguments[constructorSubstitutionCount], module, parameterCount, pParameters, parameterType
            ))
                throw(constructionParameterConstructorArgumentPrintError);
            pSubstitutions[typeSubstitutionCount + constructorSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pData->pArguments[constructorSubstitutionCount]
            };
            continue;

        constructionParameterConstructorArgumentPrintError:
            destroyExpression(parameterType);
        constructionParameterConstructorSubstituteError:
            throw(constructionConstructorSubstitutionsError);
        }
    
        for (size_t i = 0; i < constructorSubstitutionCount; i++)
            destroyExpression(pSubstitutions[typeSubstitutionCount + i].type);
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
        return true;

    constructionConstructorSubstitutionsError:
        for (size_t i = 0; i < constructorSubstitutionCount; i++)
            destroyExpression(pSubstitutions[typeSubstitutionCount + i].type);
    constructionTypeSubstitutionsError:
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
    constructionSubstitutionsMallocError:
    constructionNamePrintError:
    constructionTypeError:
        return false;
    }
    if (expression.kind == EVALUATION_EXPRESSION) {
        Evaluation* pData = expression.pData;
        if (fputc('(', stdout) == EOF)
            throw(evaluationDollarSignPrintError);
        Expression evaluationType;
        if (!evaluation_print(*pData, module, parameterCount, pParameters, &evaluationType))
            throw(evaluationPrintError);
        if (fputc(')', stdout) == EOF)
            throw(evaluationEndError);
        destroyExpression(evaluationType);
        return true;
    
    evaluationEndError:
        destroyExpression(evaluationType);
    evaluationDollarSignPrintError:
    evaluationPrintError:
        return false;
    }
    return false;
}
bool type_print(
    Expression expression, Module module, size_t parameterCount, Parameter const* pParameters
) {
    Construction universeTypeConstruction = {
        .index = 0,
        .argumentCount = 0,
        .pArguments = NULL
    };
    Expression universeType = {
        .kind = CONSTRUCTION_EXPRESSION,
        .pData = &universeTypeConstruction
    };
    return expression_print(expression, module, parameterCount, pParameters, universeType);
}
bool evaluation_print(
    Evaluation evaluation, Module module, size_t parameterCount, Parameter const* pParameters, Expression* pType
) {
    if (evaluation.kind == REFERENCE_EVALUATION) {
        size_t* pData = evaluation.pData;
        String name = pParameters[*pData].name;
        if (!string_print(name))
            throw(referenceNamePrintError);
        
        Expression type;
        if (!expression_duplicate(pParameters[*pData].type, &type))
            throw(referenceTypeDuplicateError);
        
        *pType = type;
        return true;
    
        destroyExpression(type);
    referenceTypeDuplicateError:
    referenceNamePrintError:
        return false;
    }
    if (evaluation.kind == DESTRUCTION_EVALUATION) {
        Destruction* pData = evaluation.pData;
        
        Expression type;
        
        if (!evaluation_print(pData->caller, module, parameterCount, pParameters, &type))
            throw(destructionEvaluationPrintError);
        if (fputc('.', stdout) == EOF)
            throw(destructionPeriodPrintError);
        if (type.kind != CONSTRUCTION_EXPRESSION)
            throw(destructionCallerTypeError);
        Construction* pTypeConstruction = type.pData;
        Constructor typeConstructor = module.pMatrices[0].pConstructors[pTypeConstruction->index];
        Matrix matrix = module.pMatrices[pTypeConstruction->index];
        
        Destructor destructor = matrix.pDestructors[pData->index];
        if (!string_print(destructor.name))
            throw(destructionDestructorNamePrintError);
    
        Substitution* pSubstitutions = malloc(
            (typeConstructor.parameterCount + 1 + destructor.parameterCount) * sizeof(Substitution)
        );
        if (pSubstitutions == NULL)
            throw(destructionSubstitutionsMallocError);
        size_t typeSubstitutionCount;
        for (
            typeSubstitutionCount = 0;
            typeSubstitutionCount < typeConstructor.parameterCount;
            typeSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                typeConstructor.pParameterTypes[typeSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(destructionParameterTypeSubstituteError);
        
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pTypeConstruction->pArguments[typeSubstitutionCount]
            };
            continue;
        
            destroyExpression(parameterType);
        destructionParameterTypeSubstituteError:
            throw(destructionTypeSubstitutionsError);
        }
        pSubstitutions[typeSubstitutionCount] = (Substitution) {
            .value = {
                .kind = EVALUATION_EXPRESSION,
                .pData = &pData->caller
            },
            .type = type
        };
        size_t destructorSubstitutionCount;
        for (
            destructorSubstitutionCount = 0;
            destructorSubstitutionCount < destructor.parameterCount;
            destructorSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                destructor.pParameterTypes[destructorSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(destructionParameterDestructorSubstituteError);
        
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pData->pArguments[destructorSubstitutionCount]
            };
            if (fputc(' ', stdout) == EOF)
                throw(destructionParameterConstructorArgumentPrintError);
            if (!expression_print(
                pData->pArguments[destructorSubstitutionCount], module, parameterCount, pParameters, parameterType
            ))
                throw(destructionParameterConstructorArgumentPrintError);
            pSubstitutions[typeSubstitutionCount + 1 + destructorSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pData->pArguments[destructorSubstitutionCount]
            };
            continue;
    
        destructionParameterConstructorArgumentPrintError:
            destroyExpression(parameterType);
        destructionParameterDestructorSubstituteError:
            throw(destructionDestructorSubstitutionsError);
        }
        
        Expression resultType;
        if (!expression_substitute(destructor.returnType, module, pSubstitutions, &resultType))
            throw(destructionResultTypeComputeError);
    
        *pType = resultType;
        destroyExpression(type);
        return true;
    
        destroyExpression(resultType);
    destructionResultTypeComputeError:
    destructionDestructorSubstitutionsError:
        for (size_t i = 0; i < destructorSubstitutionCount; i++)
            destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].type);
    destructionTypeSubstitutionsError:
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
    destructionSubstitutionsMallocError:
    destructionDestructorNamePrintError:
    destructionCallerTypeError:
    destructionPeriodPrintError:
        destroyExpression(type);
    destructionEvaluationPrintError:
        return false;
    }
    return false;
}
bool substitution_destruct(
    Substitution substitution, Module module, size_t index, Expression const* pArguments,
    Substitution* pResult
) {
    if (substitution.type.kind != CONSTRUCTION_EXPRESSION)
        throw(typeKindError);
    Construction* pTypeConstruction = substitution.type.pData;
    Constructor typeConstructor = module.pMatrices[0].pConstructors[pTypeConstruction->index];
    
    Substitution* pTypeSubstitutions = malloc(typeConstructor.parameterCount * sizeof(Substitution));
    if (pTypeSubstitutions == NULL)
        throw(typeSubstitutionsMallocError);
    size_t typeSubstitutionCount;
    for (
        typeSubstitutionCount = 0;
        typeSubstitutionCount < typeConstructor.parameterCount;
        typeSubstitutionCount++
    ) {
        Expression type;
        if (!expression_substitute(
            typeConstructor.pParameterTypes[typeSubstitutionCount], module, pTypeSubstitutions,
            &type
        ))
            throw(typeSubstitutionTypeComputeError);
        pTypeSubstitutions[typeSubstitutionCount] = (Substitution) {
            .type = type,
            .value = pTypeConstruction->pArguments[typeSubstitutionCount]
        };
        continue;
    
        destroyExpression(type);
    typeSubstitutionTypeComputeError:
        throw(typeSubstitutionCreateError);
    }
    
    Destructor destructor = module.pMatrices[pTypeConstruction->index].pDestructors[index];
    Substitution* pDestructorSubstitutions = malloc(
        (typeSubstitutionCount + 1 + destructor.parameterCount) * sizeof(Substitution)
    );
    if (pDestructorSubstitutions == NULL)
        throw(destructorSubstitutionsMallocError);
    memcpy(pDestructorSubstitutions, pTypeSubstitutions, typeSubstitutionCount * sizeof(Substitution));
    pDestructorSubstitutions[typeSubstitutionCount] = substitution;
    size_t destructorSubstitutionCount;
    for (
        destructorSubstitutionCount = typeSubstitutionCount + 1;
        destructorSubstitutionCount < typeSubstitutionCount + 1 + destructor.parameterCount;
        destructorSubstitutionCount++
    ) {
        Expression type;
        if (!expression_substitute(
            destructor.pParameterTypes[
                destructorSubstitutionCount - typeSubstitutionCount - 1
            ], module, pDestructorSubstitutions,
            &type
        ))
            throw(destructorSubstitutionTypeComputeError);
        pDestructorSubstitutions[destructorSubstitutionCount] = (Substitution) {
            .type = type,
            .value = pArguments[destructorSubstitutionCount - typeSubstitutionCount - 1]
        };
        continue;
    
        destroyExpression(type);
    destructorSubstitutionTypeComputeError:
        throw(destructorSubstitutionCreateError);
    }
    
    Expression type;
    if (!expression_substitute(destructor.returnType, module, pDestructorSubstitutions, &type))
        throw(returnTypeSubstituteError);
    
    Expression value;
    if (substitution.value.kind == CONSTRUCTION_EXPRESSION) {
        Construction* pData = substitution.value.pData;
        Constructor constructor = module.pMatrices[pTypeConstruction->index].pConstructors[pData->index];
        
        Substitution* pRuleSubstitutions = malloc((
            typeSubstitutionCount + constructor.parameterCount + destructorSubstitutionCount
        ) * sizeof(Substitution));
        if (pRuleSubstitutions == NULL)
            throw(constructionRuleSubstitutionsMallocError);
        memcpy(pRuleSubstitutions, pTypeSubstitutions, typeSubstitutionCount * sizeof(Substitution));
        memcpy(
            &pRuleSubstitutions[typeSubstitutionCount + constructor.parameterCount],
            &pDestructorSubstitutions[typeSubstitutionCount + 1], destructorSubstitutionCount * sizeof(Substitution)
        );
        size_t ruleSubstitutionCount;
        for (
            ruleSubstitutionCount = typeSubstitutionCount + destructorSubstitutionCount;
            ruleSubstitutionCount < typeSubstitutionCount + destructorSubstitutionCount + constructor.parameterCount;
            ruleSubstitutionCount++
        ) {
            Expression argumentType;
            if (!expression_substitute(
                constructor.pParameterTypes[
                    ruleSubstitutionCount - typeSubstitutionCount - destructorSubstitutionCount
                ], module, pRuleSubstitutions,
                &argumentType
            ))
                throw(ruleSubstitutionTypeComputeError);
            pRuleSubstitutions[ruleSubstitutionCount - destructorSubstitutionCount] = (Substitution) {
                .type = argumentType,
                .value = pData->pArguments[ruleSubstitutionCount - typeSubstitutionCount - destructorSubstitutionCount]
            };
            continue;
    
            destroyExpression(type);
        ruleSubstitutionTypeComputeError:
            throw(ruleSubstitutionCreateError);
        }
        
        if (destructor.pRules[pData->index].kind == UNSPECIFIED_EXPRESSION)
            throw(constructionRuleUnspecifiedError);
        if (!expression_substitute(destructor.pRules[pData->index], module, pRuleSubstitutions, &value))
            throw(constructionValueCreateError);
        for (size_t i = typeSubstitutionCount + destructorSubstitutionCount; i < ruleSubstitutionCount; i++)
            destroyExpression(pRuleSubstitutions[i - destructorSubstitutionCount].type);
        free(pRuleSubstitutions);
        goto valueCreateSuccess;
    
        destroyExpression(value);
    constructionValueCreateError:
    constructionRuleUnspecifiedError:
    ruleSubstitutionCreateError:
        for (size_t i = typeSubstitutionCount + destructorSubstitutionCount; i < ruleSubstitutionCount; i++)
            destroyExpression(pRuleSubstitutions[i - destructorSubstitutionCount].type);
        free(pRuleSubstitutions);
    constructionRuleSubstitutionsMallocError:
        throw(valueCreateError);
    }
    if (substitution.value.kind == EVALUATION_EXPRESSION) {
        Evaluation* pData = substitution.value.pData;
        
        Evaluation caller;
        if (!evaluation_duplicate(*pData, &caller))
            throw(evaluationDuplicateError);
    
        Expression* pArgumentCopies = malloc(destructor.parameterCount * sizeof(Expression));
        if (pArgumentCopies == NULL)
            throw(evaluationArgumentCopiesMallocError);
        size_t argumentCopyCount;
        for (argumentCopyCount = 0; argumentCopyCount < destructor.parameterCount; argumentCopyCount++) {
            if (!expression_duplicate(pArguments[argumentCopyCount], &pArgumentCopies[argumentCopyCount]))
                throw(evaluationArgumentDuplicateError);
        }
        
        Destruction destruction = {
            .caller = caller,
            .index = index,
            .argumentCount = argumentCopyCount,
            .pArguments = pArgumentCopies
        };
        Evaluation evaluation;
        if (!createDestructionEvaluation(destruction, &evaluation))
            throw(evaluationCreateError);
        if (!createEvaluationExpression(evaluation, &value))
            throw(evaluationValueCreateError);
        goto valueCreateSuccess;
    
        destroyExpression(value);
    evaluationValueCreateError:
        destroyEvaluation(evaluation);
    evaluationCreateError:
    evaluationArgumentDuplicateError:
        for (size_t i = 0; i < argumentCopyCount; i++)
            destroyExpression(pArgumentCopies[i]);
        free(pArgumentCopies);
    evaluationArgumentCopiesMallocError:
        destroyEvaluation(caller);
    evaluationDuplicateError:
        throw(valueCreateError);
    }
    throw(valueKindError);
    
valueCreateSuccess:
    *pResult = (Substitution) {
        .type = type,
        .value = value
    };
    for (size_t i = typeSubstitutionCount + 1; i < destructorSubstitutionCount; i++)
        destroyExpression(pDestructorSubstitutions[i].type);
    free(pDestructorSubstitutions);
    for (size_t i = 0; i < typeSubstitutionCount; i++)
        destroyExpression(pTypeSubstitutions[i].type);
    free(pTypeSubstitutions);
    return true;
    
valueCreateError:
valueKindError:
    destroyExpression(type);
returnTypeSubstituteError:
destructorSubstitutionCreateError:
    for (size_t i = typeSubstitutionCount + 1; i < destructorSubstitutionCount; i++)
        destroyExpression(pDestructorSubstitutions[i].type);
    free(pDestructorSubstitutions);
destructorSubstitutionsMallocError:
typeSubstitutionCreateError:
    for (size_t i = 0; i < typeSubstitutionCount; i++)
        destroyExpression(pTypeSubstitutions[i].type);
    free(pTypeSubstitutions);
typeSubstitutionsMallocError:
typeKindError:
    return false;
}
bool parser_parseExpression(
    Parser* pParser, Module module,
    size_t parameterCount, Parameter const* pParameters, Expression type,
    Expression* pExpression
) {
    if (
        isalnum(pParser->next) ||
        pParser->next == '_' ||
        pParser->next == '+' ||
        pParser->next == '-' ||
        pParser->next == '*' ||
        pParser->next == '/' ||
        pParser->next == '%' ||
        pParser->next == '^' ||
        pParser->next == '&' ||
        pParser->next == '=' ||
        pParser->next == '\'' ||
        pParser->next == '"' ||
        pParser->next == '\\' ||
        pParser->next == ',' ||
        pParser->next == '`' ||
        pParser->next == '?'
    ) {
        if (type.kind != CONSTRUCTION_EXPRESSION)
            throw(constructionTypeError);
        Construction* pTypeConstruction = type.pData;
        Constructor typeConstructor = module.pMatrices[0].pConstructors[pTypeConstruction->index];
        Matrix matrix = module.pMatrices[pTypeConstruction->index];
    
        if (pParser->next == '?') {
            for (size_t i = 0; i < parameterCount; i++) {
                if (!type_print(pParameters[i].type, module, parameterCount, pParameters))
                    throw(constructionQuestionMarkError);
                fprintf(stdout, " [%s]\n", pParameters[i].name.pData);
            }
            fprintf(stdout, "~ ");
            if (!type_print(type, module, parameterCount, pParameters))
                throw(constructionQuestionMarkError);
            fprintf(stdout, "\n");
            for (size_t i = 0; i < matrix.constructorCount; i++)
                fprintf(stdout, "|%s\n", matrix.pConstructors[i].name.pData);
            fprintf(stdout, "\n");
            throw(constructionQuestionMarkError);
        }
        
        String name;
        if (!parser_parseName(pParser, &name))
            throw(constructionNameParseError);
        
        size_t index;
        for (index = 0; index < matrix.constructorCount; index++) {
            Constructor constructor = matrix.pConstructors[index];
            if (string_equals(name, constructor.name))
                break;
        }
        if (index == matrix.constructorCount)
            throw(constructionNameError);
        Constructor constructor = matrix.pConstructors[index];
        
        Substitution* pSubstitutions = malloc(
            (typeConstructor.parameterCount + constructor.parameterCount) * sizeof(Substitution)
        );
        if (pSubstitutions == NULL)
            throw(constructionSubstitutionsMallocError);
        size_t typeSubstitutionCount;
        for (
            typeSubstitutionCount = 0;
            typeSubstitutionCount < typeConstructor.parameterCount;
            typeSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                typeConstructor.pParameterTypes[typeSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(constructionParameterTypeSubstituteError);
            
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pTypeConstruction->pArguments[typeSubstitutionCount]
            };
            continue;
    
            destroyExpression(parameterType);
        constructionParameterTypeSubstituteError:
            throw(constructionTypeSubstitutionsError);
        }
        size_t constructorSubstitutionCount;
        for (
            constructorSubstitutionCount = 0;
            constructorSubstitutionCount < constructor.parameterCount;
            constructorSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                constructor.pParameterTypes[constructorSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(constructionParameterConstructorSubstituteError);
            
            Expression parameterValue;
            if (!parser_parseExpression(pParser, module, parameterCount, pParameters, parameterType, &parameterValue))
                throw(constructionParameterValueParseError);
        
            pSubstitutions[typeSubstitutionCount + constructorSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = parameterValue
            };
            continue;
    
            destroyExpression(parameterValue);
        constructionParameterValueParseError:
            destroyExpression(parameterType);
        constructionParameterConstructorSubstituteError:
            throw(constructionConstructorSubstitutionsError);
        }
        
        Expression* pArguments = malloc(constructorSubstitutionCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(constructionArgumentsMallocError);
        for (size_t i = 0; i < constructorSubstitutionCount; i++)
            pArguments[i] = pSubstitutions[typeSubstitutionCount + i].value;
    
        Construction construction = {
            .index = index,
            .argumentCount = constructorSubstitutionCount,
            .pArguments = pArguments
        };
        Expression expression;
        if (!createConstructionExpression(construction, &expression))
            throw(constructionExpressionCreateError);
        
        *pExpression = expression;
        for (size_t i = 0; i < constructorSubstitutionCount; i++)
            destroyExpression(pSubstitutions[typeSubstitutionCount + i].type);
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
        return true;
    
        destroyExpression(expression);
    constructionExpressionCreateError:
        free(pArguments);
    constructionArgumentsMallocError:
    constructionConstructorSubstitutionsError:
        for (size_t i = 0; i < constructorSubstitutionCount; i++) {
            destroyExpression(pSubstitutions[typeSubstitutionCount + i].value);
            destroyExpression(pSubstitutions[typeSubstitutionCount + i].type);
        }
    constructionTypeSubstitutionsError:
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
    constructionSubstitutionsMallocError:
    constructionNameError:
        destroyString(name);
    constructionNameParseError:
    constructionQuestionMarkError:
    constructionTypeError:
        return false;
    }
    Substitution caller;
    bool isAnnotation = false;
    if (pParser->next == '$') {
        isAnnotation = true;
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        if (!parser_parseType(pParser, module, parameterCount, pParameters, &caller.type))
            throw(annotationTypeParseError);
        
        if (pParser->next != '[')
            throw(annotationColonError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        if (!parser_parseExpression(pParser, module, parameterCount, pParameters, caller.type, &caller.value))
            throw(annotationExpressionParseError);
        
        goto callerParseSuccess;
    
        destroyExpression(caller.value);
    annotationExpressionParseError:
    annotationColonError:
        destroyExpression(caller.type);
    annotationTypeParseError:
        throw(callerParseError);
    }
    if (pParser->next == '(') {
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
    
        if (pParser->next == '?') {
            for (size_t i = 0; i < parameterCount; i++) {
                if (!type_print(pParameters[i].type, module, parameterCount, pParameters))
                    throw(parameterQuestionMarkError);
                fprintf(stdout, " [%s]\n", pParameters[i].name.pData);
            }
            fprintf(stdout, "~ ");
            if (!type_print(type, module, parameterCount, pParameters))
                throw(parameterQuestionMarkError);
            fprintf(stdout, "\n\n");
            throw(parameterQuestionMarkError);
        }
        
        String name;
        if (!parser_parseWord(pParser, &name))
            throw(parameterNameParseError);
        
        size_t index;
        for (index = 0; index < parameterCount; index++) {
            if (string_equals(name, pParameters[index].name))
                break;
        }
        if (index == parameterCount)
            throw(parameterNameError);
        
        if (!expression_duplicate(pParameters[index].type, &caller.type))
            throw(parameterTypeDuplicateError);
        
        Evaluation evaluation;
        if (!createReferenceEvaluation(index, &evaluation))
            throw(parameterEvaluationCreateError);
        if (!createEvaluationExpression(evaluation, &caller.value))
            throw(parameterExpressionCreateError);
        goto callerParseSuccess;
    
        destroyExpression(caller.value);
    parameterExpressionCreateError:
        destroyEvaluation(evaluation);
    parameterEvaluationCreateError:
        destroyExpression(caller.type);
    parameterTypeDuplicateError:
    parameterNameError:
        destroyString(name);
    parameterNameParseError:
    parameterQuestionMarkError:
        throw(callerParseError);
    }
    throw(invalidSymbolError);

callerParseSuccess:
    while (pParser->next == '.') {
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        if (caller.type.kind != CONSTRUCTION_EXPRESSION)
            throw(destructionTypeError);
        Construction* pTypeConstruction = caller.type.pData;
        Constructor typeConstructor = module.pMatrices[0].pConstructors[pTypeConstruction->index];
        Matrix matrix = module.pMatrices[pTypeConstruction->index];
    
        if (pParser->next == '?') {
            for (size_t i = 0; i < parameterCount; i++) {
                if (!type_print(pParameters[i].type, module, parameterCount, pParameters))
                    throw(destructionQuestionMarkError);
                fprintf(stdout, " [%s]\n", pParameters[i].name.pData);
            }
            fprintf(stdout, "~ ");
            if (!type_print(caller.type, module, parameterCount, pParameters))
                throw(destructionQuestionMarkError);
            fprintf(stdout, "\n");
            for (size_t i = 0; i < matrix.destructorCount; i++)
                fprintf(stdout, ".%s\n", matrix.pDestructors[i].name.pData);
            fprintf(stdout, "\n");
            throw(destructionQuestionMarkError);
        }
        
        String name;
        if (!parser_parseName(pParser, &name))
            throw(destructionNameParseError);
        
        size_t index;
        for (index = 0; index < matrix.destructorCount; index++) {
            Destructor destructor = matrix.pDestructors[index];
            if (string_equals(destructor.name, name))
                break;
        }
        if (index == matrix.destructorCount)
            throw(destructionNameError);
        Destructor destructor = matrix.pDestructors[index];
        
        Substitution* pSubstitutions = malloc(
            (typeConstructor.parameterCount + 1 + destructor.parameterCount) * sizeof(Substitution)
        );
        if (pSubstitutions == NULL)
            throw(destructionSubstitutionsMallocError);
        size_t typeSubstitutionCount;
        for (
            typeSubstitutionCount = 0;
            typeSubstitutionCount < typeConstructor.parameterCount;
            typeSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                typeConstructor.pParameterTypes[typeSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(destructionParameterTypeSubstituteError);
        
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = pTypeConstruction->pArguments[typeSubstitutionCount]
            };
            continue;
        
            destroyExpression(parameterType);
        destructionParameterTypeSubstituteError:
            throw(destructionTypeSubstitutionsError);
        }
        pSubstitutions[typeSubstitutionCount] = caller;
        size_t destructorSubstitutionCount;
        for (
            destructorSubstitutionCount = 0;
            destructorSubstitutionCount < destructor.parameterCount;
            destructorSubstitutionCount++
        ) {
            Expression parameterType;
            if (!expression_substitute(
                destructor.pParameterTypes[destructorSubstitutionCount], module, pSubstitutions, &parameterType
            ))
                throw(destructionParameterDestructorSubstituteError);
            
            Expression parameterValue;
            if (!parser_parseExpression(pParser, module, parameterCount, pParameters, parameterType, &parameterValue))
                throw(destructionParameterValueParseError);
            
            pSubstitutions[typeSubstitutionCount + 1 + destructorSubstitutionCount] = (Substitution) {
                .type = parameterType,
                .value = parameterValue
            };
            continue;
    
            destroyExpression(parameterValue);
        destructionParameterValueParseError:
            destroyExpression(parameterType);
        destructionParameterDestructorSubstituteError:
            throw(destructionDestructorSubstitutionsError);
        }
        
        Expression* pArguments = malloc(destructorSubstitutionCount * sizeof(Expression));
        if (pArguments == NULL)
            throw(destructionArgumentsMallocError);
        for (size_t i = 0; i < destructorSubstitutionCount; i++)
            pArguments[i] = pSubstitutions[typeSubstitutionCount + 1 + i].value;
        
        Substitution newCaller;
        if (!substitution_destruct(caller, module, index, pArguments, &newCaller))
            throw(destructionDestructError);
    
        destroyExpression(caller.value);
        destroyExpression(caller.type);
        caller = newCaller;
        free(pArguments);
        for (size_t i = 0; i < destructorSubstitutionCount; i++) {
            destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
            destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].type);
        }
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
        continue;
    
        destroyExpression(newCaller.value);
        destroyExpression(newCaller.type);
    destructionDestructError:
        free(pArguments);
    destructionArgumentsMallocError:
    destructionDestructorSubstitutionsError:
        for (size_t i = 0; i < destructorSubstitutionCount; i++) {
            destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
            destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].type);
        }
    destructionTypeSubstitutionsError:
        for (size_t i = 0; i < typeSubstitutionCount; i++)
            destroyExpression(pSubstitutions[i].type);
        free(pSubstitutions);
    destructionSubstitutionsMallocError:
    destructionNameError:
        destroyString(name);
    destructionNameParseError:
    destructionTypeError:
    destructionQuestionMarkError:
        throw(destructionParseError);
    }
    if ((!isAnnotation && pParser->next != ')') || (isAnnotation && pParser->next != ']'))
        throw(evaluationEndError);
    parser_advance(pParser);
    parser_skipWhitespace(pParser);
    
    if (!expression_equals(caller.type, type))
        throw(typeMismatchError);
    *pExpression = caller.value;
    return true;
    
typeMismatchError:
evaluationEndError:
destructionParseError:
    destroyExpression(caller.value);
    destroyExpression(caller.type);
callerParseError:
invalidSymbolError:
    return false;
}
bool parser_parseType(
    Parser* pParser, Module module,
    size_t parameterCount, Parameter const* pParameters,
    Expression* pExpression
) {
    Construction universeTypeConstruction = {
        .index = 0,
        .argumentCount = 0,
        .pArguments = NULL
    };
    Expression universeType = {
        .kind = CONSTRUCTION_EXPRESSION,
        .pData = &universeTypeConstruction
    };
    return parser_parseExpression(pParser, module, parameterCount, pParameters, universeType, pExpression);
}
bool parser_parseStatement(Parser* pParser, Module* pModule, size_t depth) {
    if (pParser->next == '<') {
        parser_advance(pParser);
        String fileName;
        if (!parser_parseFileName(pParser, &fileName))
            throw(fileNameParseError);
        if (pParser->next != '>')
            throw(fileNameEndError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        if (!parseFile(fileName.pData, pModule, depth))
            throw(fileParseEndError);
        
        destroyString(fileName);
        return true;

    fileParseEndError:
    fileNameEndError:
        destroyString(fileName);
    fileNameParseError:
        return false;
    }
    if (pParser->next == '@') {
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        String name;
        if (!parser_parseName(pParser, &name))
            throw(namespaceNameParseError);
        
        if (pParser->next != '{')
            throw(namespaceBeginError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
    
        while (pParser->next != EOF && pParser->next != '}') {
            if (!parser_parseStatement(pParser, pModule, depth + 1))
                throw(namespaceStatementParseError);
        }
    
        if (!module_endNamespace(pModule, depth + 1, name.pData))
            throw(namespaceEndError);
        if (pParser->next != '}')
            throw(namespaceEndError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
    
        destroyString(name);
        return true;
    
    namespaceEndError:
    namespaceStatementParseError:
    namespaceBeginError:
        destroyString(name);
    namespaceNameParseError:
        return false;
    }
    if (pParser->next == '#') {
        parser_advance(pParser);
        while (pParser->next != EOF && pParser->next != '\n')
            parser_advance(pParser);
        parser_skipWhitespace(pParser);
        return true;
    }
    if (pParser->next == '$') {
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        Expression type;
        if (!parser_parseType(pParser, *pModule, 0, NULL, &type))
            throw(printTypeParseError);
        
        if (pParser->next != '[')
            throw(printColonError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        Expression value;
        if (!parser_parseExpression(pParser, *pModule, 0, NULL, type, &value))
            throw(printValueParseError);
        
        while (pParser->next == '.') {
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            
            if (type.kind != CONSTRUCTION_EXPRESSION)
                throw(printDestructionTypeError);
            Construction* pTypeConstruction = type.pData;
            Constructor typeConstructor = pModule->pMatrices[0].pConstructors[pTypeConstruction->index];
            Matrix matrix = pModule->pMatrices[pTypeConstruction->index];
    
            if (pParser->next == '?') {
                fprintf(stdout, "~ ");
                if (!type_print(type, *pModule, 0, NULL))
                    throw(printDestructionQuestionMarkError);
                fprintf(stdout, "\n");
                for (size_t i = 0; i < matrix.constructorCount; i++)
                    fprintf(stdout, "|%s\n", matrix.pConstructors[i].name.pData);
                fprintf(stdout, "\n");
                throw(printDestructionQuestionMarkError);
            }
        
            String name;
            if (!parser_parseName(pParser, &name))
                throw(printDestructionNameParseError);
        
            size_t index;
            for (index = 0; index < matrix.destructorCount; index++) {
                Destructor destructor = matrix.pDestructors[index];
                if (string_equals(destructor.name, name))
                    break;
            }
            if (index == matrix.destructorCount)
                throw(printDestructionNameError);
            Destructor destructor = matrix.pDestructors[index];
        
            Substitution* pSubstitutions = malloc(
                (typeConstructor.parameterCount + 1 + destructor.parameterCount) * sizeof(Substitution)
            );
            if (pSubstitutions == NULL)
                throw(printDestructionSubstitutionsMallocError);
            size_t typeSubstitutionCount;
            for (
                typeSubstitutionCount = 0;
                typeSubstitutionCount < typeConstructor.parameterCount;
                typeSubstitutionCount++
            ) {
                Expression parameterType;
                if (!expression_substitute(
                    typeConstructor.pParameterTypes[typeSubstitutionCount], *pModule, pSubstitutions, &parameterType
                ))
                    throw(printDestructionParameterTypeSubstituteError);
            
                pSubstitutions[typeSubstitutionCount] = (Substitution) {
                    .type = parameterType,
                    .value = pTypeConstruction->pArguments[typeSubstitutionCount]
                };
                continue;
            
                destroyExpression(parameterType);
            printDestructionParameterTypeSubstituteError:
                throw(printDestructionTypeSubstitutionsError);
            }
            pSubstitutions[typeSubstitutionCount] = (Substitution) {.type = type, .value = value};
            size_t destructorSubstitutionCount;
            for (
                destructorSubstitutionCount = 0;
                destructorSubstitutionCount < destructor.parameterCount;
                destructorSubstitutionCount++
            ) {
                Expression parameterType;
                if (!expression_substitute(
                    destructor.pParameterTypes[destructorSubstitutionCount], *pModule, pSubstitutions, &parameterType
                ))
                    throw(printDestructionParameterDestructorSubstituteError);
            
                Expression parameterValue;
                if (!parser_parseExpression(
                    pParser, *pModule, 0, NULL, parameterType,
                    &parameterValue
                ))
                    throw(printDestructionParameterValueParseError);
            
                pSubstitutions[typeSubstitutionCount + 1 + destructorSubstitutionCount] = (Substitution) {
                    .type = parameterType,
                    .value = parameterValue
                };
                continue;
            
                destroyExpression(parameterValue);
            printDestructionParameterValueParseError:
                destroyExpression(parameterType);
            printDestructionParameterDestructorSubstituteError:
                throw(printDestructionDestructorSubstitutionsError);
            }
        
            Expression* pArguments = malloc(destructorSubstitutionCount * sizeof(Expression));
            if (pArguments == NULL)
                throw(printDestructionArgumentsMallocError);
            for (size_t i = 0; i < destructorSubstitutionCount; i++)
                pArguments[i] = pSubstitutions[typeSubstitutionCount + 1 + i].value;
        
            Substitution newCaller;
            if (!substitution_destruct(
                (Substitution) {.type = type, .value = value}, *pModule, index, pArguments,
                &newCaller
            ))
                throw(printDestructionDestructError);
        
            destroyExpression(value);
            destroyExpression(type);
            value = newCaller.value;
            type = newCaller.type;
            free(pArguments);
            for (size_t i = 0; i < destructorSubstitutionCount; i++) {
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].type);
            }
            for (size_t i = 0; i < typeSubstitutionCount; i++)
                destroyExpression(pSubstitutions[i].type);
            free(pSubstitutions);
            continue;
        
            destroyExpression(newCaller.value);
            destroyExpression(newCaller.type);
        printDestructionDestructError:
            free(pArguments);
        printDestructionArgumentsMallocError:
        printDestructionDestructorSubstitutionsError:
            for (size_t i = 0; i < destructorSubstitutionCount; i++) {
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].type);
            }
        printDestructionTypeSubstitutionsError:
            for (size_t i = 0; i < typeSubstitutionCount; i++)
                destroyExpression(pSubstitutions[i].type);
            free(pSubstitutions);
        printDestructionSubstitutionsMallocError:
        printDestructionNameError:
            destroyString(name);
        printDestructionNameParseError:
        printDestructionTypeError:
        printDestructionQuestionMarkError:
            throw(printDestructionParseError);
        }
        if (pParser->next != ']')
            throw(printEndError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        if (pParser->next != ';')
            throw(printSemicolonError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        
        if (!expression_print(value, *pModule, 0, NULL, type))
            throw(printError);
        if (fputc('\n', stdout) == EOF)
            throw(printError);
        
        destroyExpression(value);
        destroyExpression(type);
        return true;
    
    printSemicolonError:
    printEndError:
    printDestructionParseError:
    printError:
        destroyExpression(value);
    printValueParseError:
    printColonError:
        destroyExpression(type);
    printTypeParseError:
        return false;
    }
    if (
        isalnum(pParser->next) ||
        pParser->next == '_' ||
        pParser->next == '+' ||
        pParser->next == '-' ||
        pParser->next == '*' ||
        pParser->next == '/' ||
        pParser->next == '%' ||
        pParser->next == '^' ||
        pParser->next == '&' ||
        pParser->next == '=' ||
        pParser->next == '\'' ||
        pParser->next == '"' ||
        pParser->next == '\\' ||
        pParser->next == ',' ||
        pParser->next == '`'
    ) {
        String typeName;
        if (!parser_parseName(pParser, &typeName))
            throw(typeNameParseError);
        
        Matrix typeMatrix = pModule->pMatrices[0];
        size_t typeIndex;
        for (typeIndex = 0; typeIndex < typeMatrix.constructorCount; typeIndex++) {
            Constructor constructor = typeMatrix.pConstructors[typeIndex];
            if (string_equals(constructor.name, typeName))
                break;
        }
        if (typeIndex == typeMatrix.constructorCount)
            throw(typeNameError);
        Constructor typeConstructor = typeMatrix.pConstructors[typeIndex];
        Matrix* pMatrix = &pModule->pMatrices[typeIndex];
        
        Parameter* pTypeParameters = malloc(typeConstructor.parameterCount * sizeof(Parameter));
        if (pTypeParameters == NULL)
            throw(typeParametersMallocError);
        size_t typeParameterCount;
        for (typeParameterCount = 0; typeParameterCount < typeConstructor.parameterCount; typeParameterCount++) {
            String name;
            if (pParser->next != '(')
                throw(typeParameterDollarSignError);
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            if (!parser_parseWord(pParser, &name))
                throw(typeParameterNameParseError);
            if (pParser->next != ')')
                throw(typeParameterNameEndError);
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            
            pTypeParameters[typeParameterCount] = (Parameter) {
                .type = typeConstructor.pParameterTypes[typeParameterCount],
                .name = name
            };
            continue;
    
        typeParameterNameEndError:
            destroyString(name);
        typeParameterNameParseError:
        typeParameterDollarSignError:
            throw(typeParametersParseError);
        }
        
        if (pParser->next == '|') {
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            
            String name;
            if (!parser_parseWord(pParser, &name))
                throw(constructorNameParseError);
            for (size_t i = 0; i < pMatrix->constructorCount; i++) {
                Constructor constructor = pMatrix->pConstructors[i];
                if (string_equals(constructor.name, name))
                    throw(constructorNameError);
            }
            
            size_t parameterCount = 0;
            Parameter* pParameters = NULL;
            while (pParser->next != ';') {
                Parameter* pCombinedParameters = malloc(
                    (typeParameterCount + parameterCount) * sizeof(Parameter)
                );
                if (pCombinedParameters == NULL)
                    throw(constructorCombinedParametersMallocError);
                memcpy(pCombinedParameters, pTypeParameters, typeParameterCount * sizeof(Parameter));
                memcpy(&pCombinedParameters[typeParameterCount], pParameters, parameterCount * sizeof(Parameter));
                
                Expression type;
                if (!parser_parseType(
                    pParser, *pModule, typeParameterCount + parameterCount, pCombinedParameters,
                    &type
                ))
                    throw(constructorParameterTypeParseError);
                
                if (pParser->next != '[')
                    throw(constructorParameterColonError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
    
                String parameterName;
                if (!parser_parseWord(pParser, &parameterName))
                    throw(constructorParameterNameParseError);
                for (size_t i = 0; i < typeParameterCount + parameterCount; i++) {
                    Parameter parameter = pCombinedParameters[i];
                    if (string_equals(parameter.name, parameterName))
                        throw(constructorParameterNameError);
                }
                
                if (pParser->next != ']')
                    throw(constructorParameterEndError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
                
                Parameter* pNewParameters = realloc(pParameters, (parameterCount + 1) * sizeof(Parameter));
                if (pNewParameters == NULL)
                    throw(constructorParametersReallocError);
                pParameters = pNewParameters;
                pParameters[parameterCount] = (Parameter) {
                    .type = type,
                    .name = parameterName
                };
                parameterCount++;
                free(pCombinedParameters);
                continue;
    
            constructorParametersReallocError:
            constructorParameterEndError:
            constructorParameterNameError:
                destroyString(parameterName);
            constructorParameterNameParseError:
            constructorParameterColonError:
                destroyExpression(type);
            constructorParameterTypeParseError:
                free(pCombinedParameters);
            constructorCombinedParametersMallocError:
                throw(constructorParametersParseError);
            }
            
            Expression* pParameterTypes = malloc(parameterCount * sizeof(Expression));
            if (pParameterTypes == NULL)
                throw(constructorParameterTypesMallocError);
            for (size_t i = 0; i < parameterCount; i++)
                pParameterTypes[i] = pParameters[i].type;
            
            Constructor constructor = {
                .depth = depth,
                .name = name,
                .parameterCount = parameterCount,
                .pParameterTypes = pParameterTypes
            };
            Constructor* pNewConstructors = realloc(
                pMatrix->pConstructors, (pMatrix->constructorCount + 1) * sizeof(Constructor)
            );
            if (pNewConstructors == NULL)
                throw(constructorsReallocError);
            pMatrix->pConstructors = pNewConstructors;
            for (size_t i = 0; i < pMatrix->destructorCount; i++) {
                Destructor* pDestructor = &pMatrix->pDestructors[i];
                Expression* pNewRules = realloc(
                    pDestructor->pRules, (pMatrix->constructorCount + 1) * sizeof(Expression)
                );
                if (pNewRules == NULL)
                    throw(constructorsReallocError);
                pDestructor->pRules = pNewRules;
                pDestructor->pRules[pMatrix->constructorCount] = (Expression) {
                    .kind = UNSPECIFIED_EXPRESSION,
                    .pData = NULL
                };
            }
            pMatrix->pConstructors[pMatrix->constructorCount] = constructor;
            pMatrix->constructorCount++;
            if (typeIndex == 0) {
                Matrix* pNewMatrices = realloc(pModule->pMatrices, (pModule->matrixCount + 1) * sizeof(Matrix));
                if (pNewMatrices == NULL)
                    throw(constructorMatricesReallocError);
                pModule->pMatrices = pNewMatrices;
                pModule->pMatrices[pModule->matrixCount] = (Matrix) {
                    .constructorCount = 0,
                    .pConstructors = NULL,
                    .destructorCount = 0,
                    .pDestructors = NULL
                };
                pModule->matrixCount++;
            }
    
            for (size_t i = 0; i < parameterCount; i++)
                destroyString(pParameters[i].name);
            free(pParameters);
            goto declarationParseSuccess;
    
        constructorMatricesReallocError:
        constructorsReallocError:
            free(pParameterTypes);
        constructorParameterTypesMallocError:
        constructorParametersParseError:
            for (size_t i = 0; i < parameterCount; i++) {
                destroyString(pParameters[i].name);
                destroyExpression(pParameters[i].type);
            }
            free(pParameters);
        constructorNameError:
            destroyString(name);
        constructorNameParseError:
            throw(declarationParseError);
        }
        if (pParser->next == '.') {
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
    
            String name;
            if (!parser_parseName(pParser, &name))
                throw(destructorNameParseError);
            for (size_t i = 0; i < typeMatrix.destructorCount; i++) {
                Destructor destructor = typeMatrix.pDestructors[i];
                if (string_equals(destructor.name, name))
                    throw(destructorNameError);
            }
    
            size_t parameterCount = 0;
            Parameter* pParameters = NULL;
            while (pParser->next != '~') {
                Parameter* pCombinedParameters = malloc(
                    (typeParameterCount + 1 + parameterCount) * sizeof(Parameter)
                );
                if (pCombinedParameters == NULL)
                    throw(destructorCombinedParametersMallocError);
                memcpy(pCombinedParameters, pTypeParameters, typeParameterCount * sizeof(Parameter));
                Expression* pTypeConstructionArguments = malloc(typeParameterCount * sizeof(Expression));
                if (pTypeConstructionArguments == NULL)
                    throw(destructorTypeConstructionArgumentsMallocError);
                size_t typeConstructionArgumentCount;
                for (
                    typeConstructionArgumentCount = 0;
                    typeConstructionArgumentCount < typeParameterCount;
                    typeConstructionArgumentCount++
                ) {
                    Evaluation evaluation;
                    if (!createReferenceEvaluation(typeConstructionArgumentCount, &evaluation))
                        throw(destructorTypeConstructionArgumentEvaluationCreateError);
                    Expression expression;
                    if (!createEvaluationExpression(evaluation, &expression))
                        throw(destructorTypeConstructionArgumentExpressionCreateError);
                    pTypeConstructionArguments[typeConstructionArgumentCount] = expression;
                    continue;
                    
                    destroyExpression(expression);
                destructorTypeConstructionArgumentExpressionCreateError:
                    destroyEvaluation(evaluation);
                destructorTypeConstructionArgumentEvaluationCreateError:
                    throw(destructorTypeConstructionArgumentsCreateError);
                }
                Construction typeConstruction = {
                    .index = typeIndex,
                    .argumentCount = typeParameterCount,
                    .pArguments = pTypeConstructionArguments
                };
                pCombinedParameters[typeParameterCount] = (Parameter) {
                    .type = {
                        .kind = CONSTRUCTION_EXPRESSION,
                        .pData = &typeConstruction
                    },
                    .name = {
                        .length = 0,
                        .pData = NULL
                    }
                };
                memcpy(&pCombinedParameters[typeParameterCount + 1], pParameters, parameterCount * sizeof(Parameter));
        
                Expression type;
                if (!parser_parseType(
                    pParser, *pModule, typeParameterCount + 1 + parameterCount, pCombinedParameters,
                    &type
                ))
                    throw(destructorParameterTypeParseError);
        
                if (pParser->next != '[')
                    throw(destructorParameterColonError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
        
                String parameterName;
                if (!parser_parseWord(pParser, &parameterName))
                    throw(destructorParameterNameParseError);
                for (size_t i = 0; i < typeParameterCount + 1 + parameterCount; i++) {
                    Parameter parameter = pCombinedParameters[i];
                    if (string_equals(parameter.name, parameterName))
                        throw(destructorParameterNameError);
                }
                
                if (pParser->next != ']')
                    throw(destructorParameterEndError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
        
                Parameter* pNewParameters = realloc(pParameters, (parameterCount + 1) * sizeof(Parameter));
                if (pNewParameters == NULL)
                    throw(destructorParametersReallocError);
                pParameters = pNewParameters;
                pParameters[parameterCount] = (Parameter) {
                    .type = type,
                    .name = parameterName
                };
                parameterCount++;
                for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                    destroyExpression(pTypeConstructionArguments[i]);
                free(pTypeConstructionArguments);
                free(pCombinedParameters);
                continue;
    
            destructorParametersReallocError:
            destructorParameterEndError:
            destructorParameterNameError:
                destroyString(parameterName);
            destructorParameterNameParseError:
            destructorParameterColonError:
                destroyExpression(type);
            destructorParameterTypeParseError:
            destructorTypeConstructionArgumentsCreateError:
                for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                    destroyExpression(pTypeConstructionArguments[i]);
                free(pTypeConstructionArguments);
            destructorTypeConstructionArgumentsMallocError:
                free(pCombinedParameters);
            destructorCombinedParametersMallocError:
                throw(destructorParametersParseError);
            }
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
    
            Parameter* pCombinedParameters = malloc(
                (typeParameterCount + 1 + parameterCount) * sizeof(Parameter)
            );
            if (pCombinedParameters == NULL)
                throw(destructorReturnCombinedParametersMallocError);
            memcpy(pCombinedParameters, pTypeParameters, typeParameterCount * sizeof(Parameter));
            Expression* pTypeConstructionArguments = malloc(typeParameterCount * sizeof(Expression));
            if (pTypeConstructionArguments == NULL)
                throw(destructorReturnTypeConstructionArgumentsMallocError);
            size_t typeConstructionArgumentCount;
            for (
                typeConstructionArgumentCount = 0;
                typeConstructionArgumentCount < typeParameterCount;
                typeConstructionArgumentCount++
            ) {
                Evaluation evaluation;
                if (!createReferenceEvaluation(typeConstructionArgumentCount, &evaluation))
                    throw(destructorReturnTypeConstructionArgumentEvaluationCreateError);
                Expression expression;
                if (!createEvaluationExpression(evaluation, &expression))
                    throw(destructorReturnTypeConstructionArgumentExpressionCreateError);
                pTypeConstructionArguments[typeConstructionArgumentCount] = expression;
                continue;
        
                destroyExpression(expression);
            destructorReturnTypeConstructionArgumentExpressionCreateError:
                destroyEvaluation(evaluation);
            destructorReturnTypeConstructionArgumentEvaluationCreateError:
                throw(destructorReturnTypeConstructionArgumentsCreateError);
            }
            Construction typeConstruction = {
                .index = typeIndex,
                .argumentCount = typeParameterCount,
                .pArguments = pTypeConstructionArguments
            };
            pCombinedParameters[typeParameterCount] = (Parameter) {
                .type = {
                    .kind = CONSTRUCTION_EXPRESSION,
                    .pData = &typeConstruction
                },
                .name = {
                    .length = 0,
                    .pData = NULL
                }
            };
            memcpy(&pCombinedParameters[typeParameterCount + 1], pParameters, parameterCount * sizeof(Parameter));
            
            Expression returnType;
            if (!parser_parseType(
                pParser, *pModule, typeParameterCount + 1 + parameterCount, pCombinedParameters,
                &returnType
            ))
                throw(destructorReturnTypeParseError);
            
            Expression* pParameterTypes = malloc(parameterCount * sizeof(Expression));
            if (pParameterTypes == NULL)
                throw(destructorParameterTypesMallocError);
            for (size_t i = 0; i < parameterCount; i++)
                pParameterTypes[i] = pParameters[i].type;
            
            Expression* pRules = malloc(pMatrix->constructorCount * sizeof(Expression));
            if (pRules == NULL)
                throw(destructorRulesMallocError);
            for (size_t i = 0; i < pMatrix->constructorCount; i++)
                pRules[i] = (Expression) {.kind = UNSPECIFIED_EXPRESSION, .pData = NULL};
            
            Destructor* pNewDestructors = realloc(
                pMatrix->pDestructors, (pMatrix->destructorCount + 1) * sizeof(Destructor)
            );
            if (pNewDestructors == NULL)
                throw(destructorsReallocError);
            pMatrix->pDestructors = pNewDestructors;
            pMatrix->pDestructors[pMatrix->destructorCount] = (Destructor) {
                .depth = depth,
                .name = name,
                .parameterCount = parameterCount,
                .pParameterTypes = pParameterTypes,
                .returnType = returnType,
                .pRules = pRules
            };
            pMatrix->destructorCount++;
            
            for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                destroyExpression(pTypeConstructionArguments[i]);
            free(pTypeConstructionArguments);
            free(pCombinedParameters);
            for (size_t i = 0; i < parameterCount; i++)
                destroyString(pParameters[i].name);
            free(pParameters);
            goto declarationParseSuccess;
    
        destructorsReallocError:
            free(pRules);
        destructorRulesMallocError:
            free(pParameterTypes);
        destructorParameterTypesMallocError:
            destroyExpression(returnType);
        destructorReturnTypeParseError:
        destructorReturnTypeConstructionArgumentsCreateError:
            for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                destroyExpression(pTypeConstructionArguments[i]);
            free(pTypeConstructionArguments);
        destructorReturnTypeConstructionArgumentsMallocError:
            free(pCombinedParameters);
        destructorReturnCombinedParametersMallocError:
        destructorParametersParseError:
            for (size_t i = 0; i < parameterCount; i++) {
                destroyString(pParameters[i].name);
                destroyExpression(pParameters[i].type);
            }
            free(pParameters);
        destructorNameError:
            destroyString(name);
        destructorNameParseError:
            throw(declarationParseError);
        }
        if (pParser->next == '[') {
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            String constructorName;
            if (!parser_parseName(pParser, &constructorName))
                throw(ruleConstructorNameParseError);
            size_t constructorIndex;
            for (constructorIndex = 0; constructorIndex < pMatrix->constructorCount; constructorIndex++) {
                Constructor constructor = pMatrix->pConstructors[constructorIndex];
                if (string_equals(constructor.name, constructorName))
                    break;
            }
            if (constructorIndex == pMatrix->constructorCount)
                throw(ruleConstructorNameError);
            Constructor constructor = pMatrix->pConstructors[constructorIndex];
            
            Parameter* pConstructorParameters = malloc(constructor.parameterCount * sizeof(Parameter));
            if (pConstructorParameters == NULL)
                throw(ruleConstructorParametersMallocError);
            size_t constructorParameterCount;
            for (
                constructorParameterCount = 0;
                constructorParameterCount < constructor.parameterCount;
                constructorParameterCount++
            ) {
                String name;
                if (pParser->next != '(')
                    throw(ruleConstructorParameterNameParseError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
                if (!parser_parseWord(pParser, &name))
                    throw(ruleConstructorParameterNameParseError);
                if (pParser->next != ')')
                    throw(ruleConstructorParameterNameEndError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
    
                pConstructorParameters[constructorParameterCount] = (Parameter) {
                    .type = constructor.pParameterTypes[constructorParameterCount],
                    .name = name
                };
                continue;
    
            ruleConstructorParameterNameEndError:
                destroyString(name);
            ruleConstructorParameterNameParseError:
                throw(ruleConstructorParametersParseError);
            }
            
            if (pParser->next != '.')
                throw(rulePeriodError);
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
    
            String destructorName;
            if (!parser_parseName(pParser, &destructorName))
                throw(ruleDestructorNameParseError);
            size_t destructorIndex;
            for (destructorIndex = 0; destructorIndex < pMatrix->destructorCount; destructorIndex++) {
                Destructor destructor = pMatrix->pDestructors[destructorIndex];
                if (string_equals(destructor.name, destructorName))
                    break;
            }
            if (destructorIndex == pMatrix->destructorCount)
                throw(ruleDestructorNameError);
            Destructor destructor = pMatrix->pDestructors[destructorIndex];
            if (pMatrix->pDestructors[destructorIndex].pRules[constructorIndex].kind != UNSPECIFIED_EXPRESSION)
                throw(ruleDestructorImplementationError);
            
            Parameter* pParameters = malloc(
                (typeParameterCount + constructorParameterCount + destructor.parameterCount) * sizeof(Parameter)
            );
            if (pParameters == NULL)
                throw(ruleParametersMallocError);
            memcpy(pParameters, pTypeParameters, typeParameterCount * sizeof(Parameter));
            memcpy(
                &pParameters[typeParameterCount],
                pConstructorParameters, constructorParameterCount * sizeof(Parameter)
            );
            size_t destructorParameterCount;
            for (
                destructorParameterCount = 0;
                destructorParameterCount < destructor.parameterCount;
                destructorParameterCount++
            ) {
                String name;
                if (pParser->next != '(')
                    throw(ruleDestructorParameterNameParseError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
                if (!parser_parseWord(pParser, &name))
                    throw(ruleDestructorParameterNameParseError);
                if (pParser->next != ')')
                    throw(ruleDestructorParameterNameEndError);
                parser_advance(pParser);
                parser_skipWhitespace(pParser);
                
                Substitution* pSubstitutions = malloc(
                    (typeParameterCount + 1 + destructorParameterCount) * sizeof(Substitution)
                );
                if (pSubstitutions == NULL)
                    throw(ruleDestructorSubstitutionsMallocError);
                size_t typeSubstitutionCount;
                for (
                    typeSubstitutionCount = 0;
                    typeSubstitutionCount < typeParameterCount;
                    typeSubstitutionCount++
                ) {
                    Evaluation evaluation;
                    if (!createReferenceEvaluation(typeSubstitutionCount, &evaluation))
                        throw(ruleDestructorTypeSubstitutionEvaluationCreateError);
                    Expression value;
                    if (!createEvaluationExpression(evaluation, &value))
                        throw(ruleDestructorTypeSubstitutionExpressionCreateError);
                    pSubstitutions[typeSubstitutionCount] = (Substitution) {
                        .type = pTypeParameters[typeSubstitutionCount].type,
                        .value = value
                    };
                    continue;
    
                    destroyExpression(value);
                ruleDestructorTypeSubstitutionExpressionCreateError:
                    destroyEvaluation(evaluation);
                ruleDestructorTypeSubstitutionEvaluationCreateError:
                    throw(ruleDestructorTypeSubstitutionsCreateError);
                }
                Expression* pTypeConstructionArguments = malloc(typeParameterCount * sizeof(Expression));
                if (pTypeConstructionArguments == NULL)
                    throw(ruleDestructorTypeConstructionArgumentsMallocError);
                size_t typeConstructionArgumentCount;
                for (
                    typeConstructionArgumentCount = 0;
                    typeConstructionArgumentCount < typeParameterCount;
                    typeConstructionArgumentCount++
                ) {
                    Evaluation evaluation;
                    if (!createReferenceEvaluation(typeConstructionArgumentCount, &evaluation))
                        throw(ruleDestructorTypeConstructionArgumentEvaluationCreateError);
                    Expression expression;
                    if (!createEvaluationExpression(evaluation, &expression))
                        throw(ruleDestructorTypeConstructionArgumentExpressionCreateError);
                    pTypeConstructionArguments[typeConstructionArgumentCount] = expression;
                    continue;
        
                    destroyExpression(expression);
                ruleDestructorTypeConstructionArgumentExpressionCreateError:
                    destroyEvaluation(evaluation);
                ruleDestructorTypeConstructionArgumentEvaluationCreateError:
                    throw(ruleDestructorTypeConstructionArgumentsCreateError);
                }
                Construction typeConstruction = {
                    .index = typeIndex,
                    .argumentCount = typeParameterCount,
                    .pArguments = pTypeConstructionArguments
                };
                Expression* pValueConstructionArguments = malloc(constructorParameterCount * sizeof(Expression));
                if (pValueConstructionArguments == NULL)
                    throw(ruleDestructorValueConstructionArgumentsMallocError);
                size_t valueConstructionArgumentCount;
                for (
                    valueConstructionArgumentCount = 0;
                    valueConstructionArgumentCount < constructorParameterCount;
                    valueConstructionArgumentCount++
                ) {
                    Evaluation evaluation;
                    if (!createReferenceEvaluation(
                        typeParameterCount + valueConstructionArgumentCount,
                        &evaluation
                    ))
                        throw(ruleDestructorValueConstructionArgumentEvaluationCreateError);
                    Expression expression;
                    if (!createEvaluationExpression(evaluation, &expression))
                        throw(ruleDestructorValueConstructionArgumentExpressionCreateError);
                    pValueConstructionArguments[valueConstructionArgumentCount] = expression;
                    continue;
        
                    destroyExpression(expression);
                ruleDestructorValueConstructionArgumentExpressionCreateError:
                    destroyEvaluation(evaluation);
                ruleDestructorValueConstructionArgumentEvaluationCreateError:
                    throw(ruleDestructorValueConstructionArgumentsCreateError);
                }
                Construction valueConstruction = {
                    .index = constructorIndex,
                    .argumentCount = constructorParameterCount,
                    .pArguments = pValueConstructionArguments
                };
                pSubstitutions[typeSubstitutionCount] = (Substitution) {
                    .type = {
                        .kind = CONSTRUCTION_EXPRESSION,
                        .pData = &typeConstruction
                    },
                    .value = {
                        .kind = CONSTRUCTION_EXPRESSION,
                        .pData = &valueConstruction
                    }
                };
                size_t destructorSubstitutionCount;
                for (
                    destructorSubstitutionCount = 0;
                    destructorSubstitutionCount < destructorParameterCount;
                    destructorSubstitutionCount++
                ) {
                    Evaluation evaluation;
                    if (!createReferenceEvaluation(
                        typeParameterCount + constructorParameterCount + destructorSubstitutionCount,
                        &evaluation
                    ))
                        throw(ruleDestructorDestructorSubstitutionEvaluationCreateError);
                    Expression value;
                    if (!createEvaluationExpression(evaluation, &value))
                        throw(ruleDestructorDestructorSubstitutionExpressionCreateError);
                    pSubstitutions[typeSubstitutionCount + 1 + destructorSubstitutionCount] = (Substitution) {
                        .type = pParameters[
                            typeParameterCount + constructorParameterCount + destructorSubstitutionCount
                        ].type,
                        .value = value
                    };
                    continue;
    
                    destroyExpression(value);
                ruleDestructorDestructorSubstitutionExpressionCreateError:
                    destroyEvaluation(evaluation);
                ruleDestructorDestructorSubstitutionEvaluationCreateError:
                    throw(ruleDestructorDestructorSubstitutionsCreateError);
                }
                
                Expression type;
                if (!expression_substitute(
                    destructor.pParameterTypes[destructorParameterCount], *pModule, pSubstitutions,
                    &type
                ))
                    throw(ruleDestructorTypeSubstituteError);
    
                pParameters[typeParameterCount + constructorParameterCount + destructorParameterCount] = (Parameter) {
                    .type = type,
                    .name = name
                };
                for (size_t i = 0; i < destructorSubstitutionCount; i++)
                    destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
                for (size_t i = 0; i < valueConstructionArgumentCount; i++)
                    destroyExpression(pValueConstructionArguments[i]);
                free(pValueConstructionArguments);
                for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                    destroyExpression(pTypeConstructionArguments[i]);
                free(pTypeConstructionArguments);
                free(pSubstitutions);
                continue;
    
                destroyExpression(type);
            ruleDestructorTypeSubstituteError:
            ruleDestructorDestructorSubstitutionsCreateError:
                for (size_t i = 0; i < destructorSubstitutionCount; i++)
                    destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
            ruleDestructorValueConstructionArgumentsCreateError:
                for (size_t i = 0; i < valueConstructionArgumentCount; i++)
                    destroyExpression(pValueConstructionArguments[i]);
                free(pValueConstructionArguments);
            ruleDestructorValueConstructionArgumentsMallocError:
            ruleDestructorTypeConstructionArgumentsCreateError:
                for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                    destroyExpression(pTypeConstructionArguments[i]);
                free(pTypeConstructionArguments);
            ruleDestructorTypeConstructionArgumentsMallocError:
            ruleDestructorTypeSubstitutionsCreateError:
                for (size_t i = 0; i < typeSubstitutionCount; i++)
                    destroyExpression(pSubstitutions[i].value);
                free(pSubstitutions);
            ruleDestructorSubstitutionsMallocError:
            ruleDestructorParameterNameEndError:
                destroyString(name);
            ruleDestructorParameterNameParseError:
                throw(ruleDestructorParametersParseError);
            }
    
            if (pParser->next != ']')
                throw(ruleRightParenthesisError);
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            
            if (pParser->next != '~')
                throw(ruleTildeError);
            parser_advance(pParser);
            parser_skipWhitespace(pParser);
            
            Substitution* pSubstitutions = malloc(
                (typeParameterCount + 1 + destructorParameterCount) * sizeof(Substitution)
            );
            if (pSubstitutions == NULL)
                throw(ruleReturnTypeSubstitutionsMallocError);
            size_t typeSubstitutionCount;
            for (
                typeSubstitutionCount = 0;
                typeSubstitutionCount < typeParameterCount;
                typeSubstitutionCount++
            ) {
                Evaluation evaluation;
                if (!createReferenceEvaluation(typeSubstitutionCount, &evaluation))
                    throw(ruleReturnTypeTypeSubstitutionEvaluationCreateError);
                Expression value;
                if (!createEvaluationExpression(evaluation, &value))
                    throw(ruleReturnTypeTypeSubstitutionExpressionCreateError);
                pSubstitutions[typeSubstitutionCount] = (Substitution) {
                    .type = pTypeParameters[typeSubstitutionCount].type,
                    .value = value
                };
                continue;
        
                destroyExpression(value);
            ruleReturnTypeTypeSubstitutionExpressionCreateError:
                destroyEvaluation(evaluation);
            ruleReturnTypeTypeSubstitutionEvaluationCreateError:
                throw(ruleReturnTypeTypeSubstitutionsCreateError);
            }
            Expression* pTypeConstructionArguments = malloc(typeParameterCount * sizeof(Expression));
            if (pTypeConstructionArguments == NULL)
                throw(ruleReturnTypeTypeConstructionArgumentsMallocError);
            size_t typeConstructionArgumentCount;
            for (
                typeConstructionArgumentCount = 0;
                typeConstructionArgumentCount < typeParameterCount;
                typeConstructionArgumentCount++
            ) {
                Evaluation evaluation;
                if (!createReferenceEvaluation(typeConstructionArgumentCount, &evaluation))
                    throw(ruleReturnTypeTypeConstructionArgumentEvaluationCreateError);
                Expression expression;
                if (!createEvaluationExpression(evaluation, &expression))
                    throw(ruleReturnTypeTypeConstructionArgumentExpressionCreateError);
                pTypeConstructionArguments[typeConstructionArgumentCount] = expression;
                continue;
        
                destroyExpression(expression);
            ruleReturnTypeTypeConstructionArgumentExpressionCreateError:
                destroyEvaluation(evaluation);
            ruleReturnTypeTypeConstructionArgumentEvaluationCreateError:
                throw(ruleReturnTypeTypeConstructionArgumentsCreateError);
            }
            Construction typeConstruction = {
                .index = typeIndex,
                .argumentCount = typeParameterCount,
                .pArguments = pTypeConstructionArguments
            };
            Expression* pValueConstructionArguments = malloc(constructorParameterCount * sizeof(Expression));
            if (pValueConstructionArguments == NULL)
                throw(ruleReturnTypeValueConstructionArgumentsMallocError);
            size_t valueConstructionArgumentCount;
            for (
                valueConstructionArgumentCount = 0;
                valueConstructionArgumentCount < constructorParameterCount;
                valueConstructionArgumentCount++
            ) {
                Evaluation evaluation;
                if (!createReferenceEvaluation(
                    typeParameterCount + valueConstructionArgumentCount,
                    &evaluation
                ))
                    throw(ruleReturnTypeValueConstructionArgumentEvaluationCreateError);
                Expression expression;
                if (!createEvaluationExpression(evaluation, &expression))
                    throw(ruleReturnTypeValueConstructionArgumentExpressionCreateError);
                pValueConstructionArguments[valueConstructionArgumentCount] = expression;
                continue;
        
                destroyExpression(expression);
            ruleReturnTypeValueConstructionArgumentExpressionCreateError:
                destroyEvaluation(evaluation);
            ruleReturnTypeValueConstructionArgumentEvaluationCreateError:
                throw(ruleReturnTypeValueConstructionArgumentsCreateError);
            }
            Construction valueConstruction = {
                .index = constructorIndex,
                .argumentCount = constructorParameterCount,
                .pArguments = pValueConstructionArguments
            };
            pSubstitutions[typeSubstitutionCount] = (Substitution) {
                .type = {
                    .kind = CONSTRUCTION_EXPRESSION,
                    .pData = &typeConstruction
                },
                .value = {
                    .kind = CONSTRUCTION_EXPRESSION,
                    .pData = &valueConstruction
                }
            };
            size_t destructorSubstitutionCount;
            for (
                destructorSubstitutionCount = 0;
                destructorSubstitutionCount < destructorParameterCount;
                destructorSubstitutionCount++
            ) {
                Evaluation evaluation;
                if (!createReferenceEvaluation(
                    typeParameterCount + constructorParameterCount + destructorSubstitutionCount,
                    &evaluation
                ))
                    throw(ruleReturnTypeDestructorSubstitutionEvaluationCreateError);
                Expression value;
                if (!createEvaluationExpression(evaluation, &value))
                    throw(ruleReturnTypeDestructorSubstitutionExpressionCreateError);
                pSubstitutions[typeSubstitutionCount + 1 + destructorSubstitutionCount] = (Substitution) {
                    .type = pParameters[
                        typeParameterCount + constructorParameterCount + destructorSubstitutionCount
                    ].type,
                    .value = value
                };
                continue;
        
                destroyExpression(value);
            ruleReturnTypeDestructorSubstitutionExpressionCreateError:
                destroyEvaluation(evaluation);
            ruleReturnTypeDestructorSubstitutionEvaluationCreateError:
                throw(ruleReturnTypeDestructorSubstitutionsCreateError);
            }
            
            Expression type;
            if (!expression_substitute(destructor.returnType, *pModule, pSubstitutions, &type))
                throw(ruleReturnTypeTypeSubstituteError);
            Expression rule;
            if (!parser_parseExpression(
                pParser, *pModule,
                typeParameterCount + constructorParameterCount + destructorParameterCount, pParameters,
                type, &rule
            ))
                throw(ruleResultParseError);
            
            pMatrix->pDestructors[destructorIndex].pRules[constructorIndex] = rule;
            destroyExpression(type);
            for (size_t i = 0; i < destructorSubstitutionCount; i++)
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
            for (size_t i = 0; i < valueConstructionArgumentCount; i++)
                destroyExpression(pValueConstructionArguments[i]);
            free(pValueConstructionArguments);
            for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                destroyExpression(pTypeConstructionArguments[i]);
            free(pTypeConstructionArguments);
            for (size_t i = 0; i < typeSubstitutionCount; i++)
                destroyExpression(pSubstitutions[i].value);
            free(pSubstitutions);
            for (size_t i = 0; i < destructorParameterCount; i++)
                destroyString(pParameters[typeParameterCount + constructorParameterCount + i].name);
            free(pParameters);
            for (size_t i = 0; i < constructorParameterCount; i++)
                destroyString(pConstructorParameters[i].name);
            free(pConstructorParameters);
            goto declarationParseSuccess;
    
            destroyExpression(rule);
        ruleResultParseError:
            destroyExpression(type);
        ruleReturnTypeTypeSubstituteError:
        ruleReturnTypeDestructorSubstitutionsCreateError:
            for (size_t i = 0; i < destructorSubstitutionCount; i++)
                destroyExpression(pSubstitutions[typeSubstitutionCount + 1 + i].value);
        ruleReturnTypeValueConstructionArgumentsCreateError:
            for (size_t i = 0; i < valueConstructionArgumentCount; i++)
                destroyExpression(pValueConstructionArguments[i]);
            free(pValueConstructionArguments);
        ruleReturnTypeValueConstructionArgumentsMallocError:
        ruleReturnTypeTypeConstructionArgumentsCreateError:
            for (size_t i = 0; i < typeConstructionArgumentCount; i++)
                destroyExpression(pTypeConstructionArguments[i]);
            free(pTypeConstructionArguments);
        ruleReturnTypeTypeConstructionArgumentsMallocError:
        ruleReturnTypeTypeSubstitutionsCreateError:
            for (size_t i = 0; i < typeSubstitutionCount; i++)
                destroyExpression(pSubstitutions[i].value);
            free(pSubstitutions);
        ruleReturnTypeSubstitutionsMallocError:
        ruleTildeError:
        ruleRightParenthesisError:
        ruleDestructorParametersParseError:
            for (size_t i = 0; i < destructorParameterCount; i++)
                destroyString(pParameters[typeParameterCount + constructorParameterCount + i].name);
            free(pParameters);
        ruleParametersMallocError:
        ruleDestructorImplementationError:
        ruleDestructorNameError:
            destroyString(destructorName);
        ruleDestructorNameParseError:
        rulePeriodError:
        ruleConstructorParametersParseError:
            for (size_t i = 0; i < constructorParameterCount; i++)
                destroyString(pConstructorParameters[i].name);
            free(pConstructorParameters);
        ruleConstructorParametersMallocError:
        ruleConstructorNameError:
            destroyString(constructorName);
        ruleConstructorNameParseError:
            throw(declarationParseError);
        }
        throw(declarationParseError);
    
    declarationParseSuccess:
        if (pParser->next != ';')
            throw(declarationEndError);
        parser_advance(pParser);
        parser_skipWhitespace(pParser);
        for (size_t i = 0; i < typeParameterCount; i++)
            destroyString(pTypeParameters[i].name);
        free(pTypeParameters);
        return true;
    
    declarationEndError:
    declarationParseError:
    typeParametersParseError:
        for (size_t i = 0; i < typeParameterCount; i++)
            destroyString(pTypeParameters[i].name);
        free(pTypeParameters);
    typeParametersMallocError:
    typeNameError:
        destroyString(typeName);
    typeNameParseError:
        return false;
    }
    return false;
}
bool module_endNamespace(Module* pModule, size_t depth, char const* pNamespace) {
    size_t namespaceLength = strlen(pNamespace);
    for (size_t i = 0; i < pModule->matrixCount; i++) {
        Constructor typeConstructor = pModule->pMatrices[0].pConstructors[i];
        Matrix matrix = pModule->pMatrices[i];
        for (size_t j = 0; j < matrix.constructorCount; j++) {
            Constructor* pConstructor = &matrix.pConstructors[j];
            if (pConstructor->depth == depth && typeConstructor.depth < depth) {
                String name = pConstructor->name;
                size_t newNameLength = namespaceLength + 1 + name.length;
                char* pNewNameData = malloc(newNameLength + 1);
                if (pNewNameData == NULL)
                    throw(newNameDataMallocError);
                memcpy(pNewNameData, pNamespace, namespaceLength);
                pNewNameData[namespaceLength] = ':';
                memcpy(&pNewNameData[namespaceLength + 1], name.pData, name.length);
                pNewNameData[newNameLength] = 0;
                destroyString(name);
                pConstructor->name = (String) {
                    .length = newNameLength,
                    .pData = pNewNameData
                };
            }
        }
        for (size_t j = 0; j < matrix.destructorCount; j++) {
            Destructor* pDestructor = &matrix.pDestructors[j];
            if (pDestructor->depth == depth && typeConstructor.depth < depth) {
                String name = pDestructor->name;
                size_t newNameLength = namespaceLength + 1 + name.length;
                char* pNewNameData = malloc(newNameLength + 1);
                if (pNewNameData == NULL)
                    throw(newNameDataMallocError);
                memcpy(pNewNameData, pNamespace, namespaceLength);
                pNewNameData[namespaceLength] = ':';
                memcpy(&pNewNameData[namespaceLength + 1], name.pData, name.length);
                pNewNameData[newNameLength] = 0;
                destroyString(name);
                pDestructor->name = (String) {
                    .length = newNameLength,
                    .pData = pNewNameData
                };
            }
        }
    }
    for (size_t i = 0; i < pModule->matrixCount; i++) {
        Constructor typeConstructor = pModule->pMatrices[0].pConstructors[i];
        Matrix matrix = pModule->pMatrices[i];
        for (size_t j = 0; j < matrix.constructorCount; j++) {
            Constructor* pConstructor = &matrix.pConstructors[j];
            if (pConstructor->depth == depth)
                pConstructor->depth--;
        }
        for (size_t j = 0; j < matrix.destructorCount; j++) {
            Destructor* pDestructor = &matrix.pDestructors[j];
            if (pDestructor->depth == depth)
                pDestructor->depth--;
        }
    }
    return true;
    
newNameDataMallocError:
    return false;
}
bool module_validate(Module module, size_t depth) {
    for (size_t i = 0; i < module.matrixCount; i++) {
        Constructor typeConstructor = module.pMatrices[0].pConstructors[i];
        Matrix matrix = module.pMatrices[i];
        for (size_t j = 0; j < matrix.destructorCount; j++) {
            Destructor destructor = matrix.pDestructors[j];
            for (size_t k = 0; k < matrix.constructorCount; k++) {
                Constructor constructor = matrix.pConstructors[k];
                if (constructor.depth < depth)
                    continue;
                if (destructor.depth < depth)
                    continue;
                if (destructor.pRules[k].kind == UNSPECIFIED_EXPRESSION) {
                    fprintf(
                        stderr, "Unimplemented case found: %s [%s.%s]\n",
                        typeConstructor.name.pData, constructor.name.pData, destructor.name.pData
                    );
                    return false;
                }
            }
        }
    }
    return true;
}

bool parseFile(char const* pFileName, Module* pModule, size_t depth) {
    struct stat fileStat;
    stat(pFileName, &fileStat);
    
    if (S_ISDIR(fileStat.st_mode)) {
        char* pDirectoryName = getcwd(NULL, 0);
        if (pDirectoryName == NULL)
            throw(directoryGetError);
        if (chdir(pFileName) == -1)
            throw(directoryChangeError);
        
        if (!parseFile(MAIN_FILE_NAME, pModule, depth + 1))
            throw(fileParseError);
        
        if (chdir(pDirectoryName) == -1)
            throw(directoryRestoreError);
        free(pDirectoryName);
        return true;
    
    directoryRestoreError:
    fileParseError:
    directoryChangeError:
        free(pDirectoryName);
    directoryGetError:
        return false;
    } else {
        char* pDirectoryName;
        
        Parser parser;
        if (!createParserFromFile(pFileName, &parser))
            throw(parserCreateError);
        parser_skipWhitespace(&parser);
    
        while (parser.next != EOF) {
            if (!parser_parseStatement(&parser, pModule, depth))
                throw(statementParseError);
        }
    
        destroyParser(parser);
        return true;

    statementParseError:
        pDirectoryName = getcwd(NULL, 0);
        if (pDirectoryName != NULL) {
            fprintf(stderr, "Error encountered at %s/%s:%lu:%lu\n", pDirectoryName, pFileName, parser.lineNumber, parser.columnNumber);
            free(pDirectoryName);
        }
        destroyParser(parser);
    parserCreateError:
        return false;
    }
}
