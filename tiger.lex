%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences 
 * translated into their meaning.
 */
char *getstr(const char *str)
{
	//optional: implement this function if you need it
  char *result = (char *)malloc(yyleng + 1);
  int i = 1, j = 0, len = strlen(str);
  while (i < len -1) {
    if (str[i] == '\\'){
      switch(str[i+1]){
        case 'n':result[j] = '\n';i += 2;j++;break;
        case 't':result[j] = '\t';i += 2;j++;break;
        case '\\':result[j] = '\\';i += 2;j++;break;
        case '\"':result[j] = '\"';i += 2;j++;break;
        case '^':result[j] = str[i+2] - 'A' + 1;i += 3;j++;break;
        default:
          if (str[i+1] >= '0' && str[i+1] <= '9') {
              result[j] = (str[i+1]-'0')*10*10+(str[i+2]-'0')*10+str[i+3]-'0';
              i += 4;
              j++;
              break;
         }
         else{
              i++;
              while (str[i] != '\\') i++;
              i++;
              break;
         }
      }
    }
    else{
      result[j++] = str[i++];
    }
  }
  result[j] = '\0';
	return result;
}
int comment = 0;
%}
  /* You can add lex definitions here. */
%Start COMMENT
%%
  /* 
  * Below is an example, which you can wipe out
  * and write reguler expressions and actions of your own.
  */ 

"\n" {adjust(); EM_newline(); continue;}
<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); return TYPE;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"/*" {adjust(); comment++; BEGIN COMMENT;}
<COMMENT>"/*" {adjust(); comment++;}
<COMMENT>"*/" {adjust(); comment--; if (comment == 0) { BEGIN INITIAL;}}
[ \t]+ {adjust();}
<INITIAL>\"(\\\"|[^"])*\" { adjust(); yylval.sval = getstr(yytext); return STRING;}
<INITIAL>[0-9]* { adjust(); yylval.ival = atoi(yytext); return INT; }
<INITIAL>[_a-zA-Z][_0-9a-zA-Z]* { adjust(); yylval.sval = String(yytext); return ID; }
<COMMENT>. {adjust();}
<INITIAL>. {adjust();EM_error(EM_tokPos, "illegal token");}
<COMMENT><<EOF>> {EM_error(EM_tokPos, "unclosed comments");}
<<EOF>> { adjust(); yyterminate(); }
