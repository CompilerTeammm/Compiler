#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "lrparser.tab.h"


past newAstNode(){
	past node = malloc(sizeof(ast));
	if(node == NULL)
	{
		exit(0);
	}
	memset(node, 0, sizeof(ast));
	return node;
}

void showAst(past node, int nest){
	if(node == NULL)
		return;
	int i = 0;
	if(node->nodeType == TRANSLATION_UNIT)
	{
		showTranstion(node, nest + 1);
		return;
	} 
	else if(node->nodeType == INTEGER_LITERAL)
	{
		printf("%dINTEGER_LITERAL  %d\n", nest-1, node->ivalue);
	} 
	// else if(node->nodeType == FLOATING_LITERAL)
	// {
	// 	printf("%dFLOATING_LITERAL  %f\n", nest-1, node->fvalue);
	// } 
	// else if(node->nodeType==UNARY_OPERATOR)
	// {
	// 	printf("%dUNARY_OPERATOR: '%s'\n", nest-1,node->svalue);
	// } 
	else if(node->nodeType == FUNCTION_DECL)
	{
		printf("%dFUNCTION_DECL  '%s'\n", nest-1,node->svalue);
	} 	
	else if(node->nodeType == CALL_EXPR)
	{
		printf("%dCALL_EXPR\n", nest-1);
	} 	
	else if(node->nodeType == COMPOUND_STMT)
	{
		printf("%dCOMPOUND_STMT\n", nest-1);
		node = node->right;
		showCompoundStmt(node, nest + 1);
		return;
	} 	
	else if(node->nodeType==VARIABLE_REF)
	{
		printf("%dVARIABLE_REF: '%s'\n", nest-1,node->svalue);
	} 	
	else if(node->nodeType == PARM_DECL)
	{
		showParaDecl(node);
		return;
	} 	
	else if(node->nodeType == VAR_DECL)
	{
		printf("%dVAR_DECL: '%s'\n", nest-1,node->svalue);
	} 	
	else if(node->nodeType==DECL_REF_EXPR)
	{
		printf("%dDECL_REF_EXPR: '%s'\n", nest-1,node->svalue);
	} 
	else if(node->nodeType == IF_STMT)
	{
		printf("%dIF_STMT\n", nest-1);
		showAst(node->if_cond, nest+1);
	} 
	else if(node->nodeType == INIT_LIST_EXPR)
	{
		printf("%dINIT_LIST_EXPR\n", nest-1);
	}
	else if(node->nodeType == ARRAY_SUBSCRIPT_EXPR)
	{
		printf("%dARRAY_SUBSCRIPT_EXPR\n", nest-1);
	}
	else if(node->nodeType == PAREN_EXPR)
	{
		printf("%dPAREN_EXPR\n", nest-1);
	}	
	else if(node->svalue != NULL)
	{
		printf("%d%s  '%s'\n", nest-1, node->snodeType, node->svalue);
	} 
	else{
		if(node->snodeType)
		printf("%d%s\n", nest-1, node->snodeType);
		else
		nest--;
	}
	showAst(node->left, nest+1);
	showAst(node->right, nest+1);
}



void showTranstion(past node, int nest){
	if(node == NULL){
		return;
	}
	while(node->right != NULL){
		showAst(node->left, nest);
		node = node->right;
	}
	showAst(node->left, nest);
}

void showCompoundStmt(past node, int nest){
	if(node == NULL){
		return;
	}
	while(node->right != NULL){
		showAst(node->left, nest);
		node = node->right;
	}
	showAst(node->left, nest);
}

void showParaDecl(past node){
	if(node == NULL){
		return;
	}
	int symbol = 1;
	past Stack[100];
	int top = 0;
    while(top || node){
		if(node != NULL){ 
			Stack[top++] = node;
            node = node->left;
        }else if(top != 0){   
			node = Stack[--top];
			if(symbol == 1){
				printf("%dPARM_DECL '%s'\n", symbol,node->svalue);
				symbol--;
			} else {
				printf("%dPARM_DECL '%s'\n", symbol,node->svalue);
			}
            node = node->right;
        }
    }
}


past newNode(past l,past r){
	past node = newAstNode();
	node->left = l;
	node->right = r;
	return node;
}

past newCompUnit(past l, past r){
	past node = newAstNode();
	node->nodeType = TRANSLATION_UNIT;
	node->snodeType = "TRANSLATION_UNIT";
	node->left = l;
	node->right = r;
	return node;
}

past newVarDecl(int type, char *str, past l, past r){ 
	past node = newAstNode();
	node->ivalue = type;
	node->svalue = str;
	node->nodeType = VAR_DECL;
	node->snodeType = "VAR_DECL";
	node->left = l;
	node->right = r;

	return node;	
}

past newIntList(past l, past r){
	past node = newAstNode();
	node->nodeType = INIT_LIST_EXPR;
	node->snodeType = "INIT_LIST_EXPR";
	node->left = l;
	node->right = r; 
	return node;
}

past newDeclStmt(past l, past r){
	past node = newAstNode();
	node->nodeType = DECL_STMT;
	node->snodeType = "DECL_STMT";
	node->left = l;
	node->right = r; 
	return node;
}

past newFuncDecl(int type, char* str, past l, past r){
	past node = newAstNode();
	node->ivalue = type;
	node->svalue = str;
	node->nodeType = FUNCTION_DECL;
	node->snodeType = "FUNCTION_DECL";
	node->left = l;
	node->right = r;
	return node;	
}

past newParaDecl(char* str,  past l, past r){
	past node = newAstNode();
	node->nodeType = PARM_DECL;
	node->snodeType = "PARM_DECL";
	node->svalue = str;
	node->left = l;
	node->right = r;
	return node;
}

past newCompoundStmt(past l, past r){
	past node = newAstNode();
	node->nodeType = COMPOUND_STMT;
	node->snodeType = "COMPOUND_STMT";
	node->left = l;
	node->right = r;
	return node;
}

past newBinaryOper(char* soper, int oper, past l, past r){
	past node = newAstNode();
	node->ivalue = oper;
	node->svalue = soper;
	node->nodeType = BINARY_OPERATOR;
	node->snodeType = "BINARY_OPERATOR";
	node->left = l;
	node->right = r;
	return node;
}

past newUnaryOper(char* soper, int oper, past l, past r){ 
	past node = newAstNode();	
	node->ivalue = oper;
	node->svalue = soper;
	node->nodeType = UNARY_OPERATOR;
	node->snodeType = "UNARY_OPERATOR";
	node->left = l;
	node->right = r;
	return node;
}

past newWhileStmt(past l, past r){  
	past node = newAstNode();
	node->nodeType = WHILE_STMT;
	node->snodeType = "WHILE_STMT";
	node->left = l;
	node->right = r;
	return node;
}

past newIfStmt(past if_cond, past l, past r){ 
	past node = newAstNode();
	if(r != NULL){
		node->svalue = "has else";
	} else {
		node->svalue = "no else";
	}
	node->nodeType = IF_STMT;
	node->snodeType = "IF_STMT";
	node->if_cond = if_cond;
	node->left = l;
	node->right = r;
	return node;
}

past newBreakStmt(){
	past node = newAstNode();
	node->nodeType = BREAK_STMT;
	node->snodeType = "BREAK_STMT";
	return node;
}

past newContinueStmt(){
	past node = newAstNode();
	node->nodeType = CONTINUE_STMT;
	node->snodeType = "CONTINUE_STMT";
	return node;
}

past newReturnStmt(past l, past r){   
	past node = newAstNode();
	node->nodeType = RETURN_STMT;
	node->snodeType = "RETURN_STMT";
	node->left = l;
	node->right = r;
	return node;
}

past newDeclRefExp(char* str, past l, past r){
	past node = newAstNode();
	node->svalue = str;
	node->nodeType = DECL_REF_EXPR;
	node->snodeType = "DECL_REF_EXPR";
	node->left = l;
	node->right = r;
	return node;
}

past newArraySubscriptsExp(past l, past r){
	past node = newAstNode();
	node->nodeType = ARRAY_SUBSCRIPT_EXPR;
	node->snodeType = "ARRAY_SUBSCRIPT_EXPR";
	node->left = l;
	node->right = r;
	return node;
}

past newParenExp(past l){
	past node = newAstNode();
	node->nodeType = PAREN_EXPR;
	node->snodeType = "PAREN_EXPR";
	node->left = l;
	return node;
}

past newIntVal(int iValue){
	past node = newAstNode();
	node->ivalue = iValue;
	node->nodeType = INTEGER_LITERAL;
	node->snodeType = "INTEGER_LITERAL";
	return node;
}

past newFloatVal(float fValue){
	past node = newAstNode();
	node->fvalue = fValue;
	node->nodeType = FLOATING_LITERAL;
	node->snodeType = "FLOATING_LITERAL";
	return node;
}

past newCallExp(int type, char* str, past l, past r){
	past node = newAstNode();
	node->ivalue = type;
	node->svalue = str;
	node->nodeType = CALL_EXPR;
	node->snodeType = "CALL_EXPR";
	node->left = l;
	node->right = r;
	return node;
}

past newType(int oper){
	past node = newAstNode();
	node->ivalue = oper;
	return node;
}

past newVarRef(char* str){ 
	past node = newAstNode();
	node->svalue = str;
	node->nodeType = VARIABLE_REF;
	node->snodeType = "VARIABLE_REF";
	return node;
}



void yyerror(char *s)
{
    printf("%s",s);
}

extern int yyparse();
extern FILE *yyin;
int main(int argc, char **argv)
{
    if (argc > 2)
    {
        return 0;
    }
    if (argc == 2)
    {
        yyin = fopen(argv[1], "r");
				yyparse();
        fclose(yyin);
    }
		yyparse();
    return 0;
}
