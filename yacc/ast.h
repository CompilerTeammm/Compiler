#ifndef COMPILER_LAB_NODE_TYPE_H
#define COMPILER_LAB_NODE_TYPE_H

enum _node_type{

	// 未通过此接口公开其特定种类的声明
	//

	// C 或 C++ 结构体
	STRUCT_DECL = 2,

	// C 或 C++ 联合体
	UNION_DECL = 3,

	// 枚举
	ENUM_DECL = 5,

	// 结构体、联合体或类中的字段（对应于C中的field）或非静态数据成员（对应于C++中的非静态数据成员）
	FIELD_DECL = 6,

	// 枚举常量
	ENUM_CONSTANT_DECL = 7,

	// 函数
	FUNCTION_DECL = 8,

	// 变量
	VAR_DECL = 9,

	// 函数或方法的参数
	PARM_DECL = 10,


	// 类型定义
	TYPEDEF_DECL = 20,

	// 类型别名声明
	TYPE_ALIAS_DECL = 36,


	// 对结构体、联合体或类的成员的引用，出现在一些非表达式上下文中，例如指定的初始化
	MEMBER_REF = 47,

	// 对标记语句的引用
	LABEL_REF = 48,

	// 对尚未解析为特定函数或函数模板的一组重载函数或函数模板的引用
	OVERLOADED_DECL_REF = 49,

	// 在某些非表达式上下文中出现的对变量的引用，例如C++的lambda捕获列表
	VARIABLE_REF = 50,


	//////
	// 无效/错误类型

	INVALID_FILE = 70,
	NO_DECL_FOUND = 71,
	NOT_IMPLEMENTED = 72,
	INVALID_CODE = 73,
	

	//////
	// 表达式类型

	// 未公开特定种类的表达式
	//
	// 未公开的表达式具有与任何其他类型的表达式相同的操作；可以提取它们的位置信息、拼写、子项等。
	// 但是，不会报告表达式的具体类型
	UNEXPOSED_EXPR = 100,

	// 引用某个值声明的表达式，如函数、变量或枚举器
	DECL_REF_EXPR = 101,

	// 引用结构体、联合体、类、Objective-C类等的成员的表达式
	MEMBER_REF_EXPR = 102,

	// 调用函数的表达式
	CALL_EXPR = 103,


	// 表示块字面值的表达式
	BLOCK_EXPR = 105,

	// 整数文字
	INTEGER_LITERAL = 106,

	// 浮点数文字
	FLOATING_LITERAL = 107,

	// 虚数文字
	IMAGINARY_LITERAL = 108,

	// 字符串文字
	STRING_LITERAL = 109,

	// 字符文字
	CHARACTER_LITERAL = 110,

	// 括号表达式，例如 "1,"
	//
	// 仅在请求完整位置信息时形成此AST节点
	PAREN_EXPR = 111,

	// 这代表一元表达式，除了sizeof和alignof之外的一元表达式
	UNARY_OPERATOR = 112,

	// [C99 6521] 数组下标
	ARRAY_SUBSCRIPT_EXPR = 113,

	// 诸如 "x + y" 或 "x <= y" 之类的内建二进制操作表达式
	BINARY_OPERATOR = 114,

	// 复合赋值，例如 "+="
	COMPOUND_ASSIGNMENT_OPERATOR = 115,

	// 三元运算符 ?: 
	CONDITIONAL_OPERATOR = 116,

	// C中的显式转换或C++中的C样式转换，使用语法 Type,expr
	//
	// 例如：int,f
	CSTYLE_CAST_EXPR = 117,

	// [C99 6525]
	COMPOUND_LITERAL_EXPR = 118,

	// 描述C或C++初始化列表
	INIT_LIST_EXPR = 119,

	// GNU地址标签扩展，表示&&label
	ADDR_LABEL_EXPR = 120,



	// 其特定种类未通过此接口公开的语句
	//
	// 未公开的语句具有与任何其他类型的语句相同的操作；
	// 可以提取它们的位置信息、拼写、子项等。但是，未报告语句的具体种类
	UNEXPOSED_STMT = 200,

	// 函数中的标记语句
	LABEL_STMT = 201,

	// 复合语句
	COMPOUND_STMT = 202,

	// case 语句
	CASE_STMT = 203,

	// default 语句
	DEFAULT_STMT = 204,

	// if 语句
	IF_STMT = 205,

	// switch 语句
	SWITCH_STMT = 206,

	// while 语句
	WHILE_STMT = 207,

	// do 语句
	DO_STMT = 208,

	// for 语句
	FOR_STMT = 209,

	// goto 语句
	GOTO_STMT = 210,

	// 间接 goto 语句
	INDIRECT_GOTO_STMT = 211,

	// continue 语句
	CONTINUE_STMT = 212,

	// break 语句
	BREAK_STMT = 213,

	// return 语句
	RETURN_STMT = 214,


	// 空语句
	NULL_STMT = 230,

	// 用于将声明与语句和表达式混合的适配器类
	DECL_STMT = 231,


	//////
	// 其他种类

	// 代表翻译单元本身的游标
	//
	// 翻译单元游标主要用于作为遍历翻译单元内容的根游标
	TRANSLATION_UNIT = 300,
};


typedef enum _node_type node_type;
extern int type;

extern int yylex();
extern char* yytext;
extern int yyleng;

typedef struct _ast ast;
typedef struct _ast *past;
struct _ast{
    int ivalue;
    float fvalue;
    char* svalue;
    node_type nodeType;
    char* snodeType;
    past left;
    past right;
    past if_cond;
    past next;
};



past newAstNode();

void showAst(past node, int nest);
void showTranstion(past node, int nest);
void showCompoundStmt(past node, int nest);
void showParaDecl(past node);


past newNode(past l, past r);
past newCompUnit(past l, past r);
past newVarDecl(int type, char* s, past l, past r);
past newIntList(past l, past r);
past newDeclStmt(past l, past r);
past newFuncDecl(int type,char* svalue, past l, past r);
past newParaDecl(char* str, past l, past r);
past newCompoundStmt(past l, past r);
past newBinaryOper(char* soper, int oper, past l, past r);
past newUnaryOper(char* soper, int oper, past l, past r);
past newWhileStmt(past l, past r);
past newIfStmt(past if_cond, past l, past r);
past newBreakStmt();
past newContinueStmt();
past newReturnStmt(past l, past r);
past newDeclRefExp(char* str, past l, past r);
past newArraySubscriptsExp(past l, past r);
past newParenExp(past l);
past newIntVal(int iValue);
past newFloatVal(float fValue);
past newCallExp(int type, char* str, past l, past r);
past newType(int oper);
past newVarRef(char* str);


void yyerror(char *s);

#endif