%{ /* -*- C++ -*- */
    #include <cstdlib>
    #include <string>
    #include <cerrno>
    #include <climits>
    #include <cmath>

    #include "parser.hpp"
    #include "driver.hpp"
%}

%option noyywrap nounput batch debug noinput

id     [a-zA-Z][a-zA-Z_0-9]*
fpnum  [0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
fixnum (0|[1-9][0-9]*)\.?[0-9]*
num    {fpnum}|{fixnum}
blnk   [ \t]

%{
  #define YY_USER_ACTION loc.columns(yyleng);
%}

%%

%{
  yy::location& loc=driver.location;
  loc.step();
%}

{blnk}+     loc.step ();
[\n]+       loc.lines(yyleng); loc.step();
"+"         return yy::parser::make_ADD        (loc);
"-"         return yy::parser::make_SUB        (loc);
"*"         return yy::parser::make_MUL        (loc);
"/"         return yy::parser::make_DIV        (loc);
"="         return yy::parser::make_ASS        (loc);
"=="        return yy::parser::make_EQ         (loc);
"<"         return yy::parser::make_LT         (loc);
"{"         return yy::parser::make_OBRACE     (loc);
"}"         return yy::parser::make_CBRACE     (loc);
"("         return yy::parser::make_OPAREN     (loc);
")"         return yy::parser::make_CPAREN     (loc);
";"         return yy::parser::make_CSTMT      (loc);
","         return yy::parser::make_SEP        (loc);
"?"         return yy::parser::make_COND       (loc);
":"         return yy::parser::make_COL        (loc);
"def"       return yy::parser::make_DEF        (loc);
"extern"    return yy::parser::make_EXT        (loc);
"var"       return yy::parser::make_VAR        (loc);
"global"    return yy::parser::make_GLOB       (loc);
"if"        return yy::parser::make_IF         (loc);
"else"      return yy::parser::make_ELSE       (loc);
"for"       return yy::parser::make_FOR        (loc);
"not"       return yy::parser::make_NOT        (loc);
"and"       return yy::parser::make_AND        (loc);
"or"        return yy::parser::make_OR         (loc);
{num} {
            errno=0;
            double n=strtod(yytext, NULL);
            if(!(n!=HUGE_VAL && n!=-HUGE_VAL && errno!=ERANGE))
                throw yy::parser::syntax_error(loc,"Valore fuori dal range "+std::string(yytext));
            return yy::parser::make_NUM        (n,loc);
}
{id}        return yy::parser::make_ID         (yytext,loc);
.           throw yy::parser::syntax_error(loc,"Carattere non riconosciuto: "+std::string(yytext));
<<EOF>>     return yy::parser::make_END        (loc);
%%

void Driver::scan_begin(){
  yy_flex_debug=trace_scanning;
  if(f.empty() || f=="-")
    yyin=stdin;
  else if(!(yyin=fopen(f.c_str(),"r"))){
        std::cerr << "Errore nell'apertura di "<<f<<": "<<strerror(errno)<<'\n';
        exit(EXIT_FAILURE);
    }
}

void Driver::scan_end(){
  fclose(yyin);
}