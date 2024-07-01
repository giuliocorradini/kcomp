%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.2"
%defines

%define api.token.constructor
%define api.location.file none
%define api.value.type variant
%define parse.assert

%code requires {
  #include <string>
  #include <exception>
  class driver;
  class RootAST;
  class ExprAST;
  class NumberExprAST;
  class VariableExprAST;
  class CallExprAST;
  class FunctionAST;
  class SeqAST;
  class PrototypeAST;
  class BlockAST;
  class VarBindingAST;
  class ConditionalExprAST;
  class GlobalVarAST;
  class AssignmentAST;
  class IfExprAST;
  class IfStatementAST;
  class ForStatementAST;
  class UnaryOperatorBaseAST;
}

// The parsing context.
%param { driver& drv }

%locations

%define parse.trace
%define parse.error verbose

%code {
# include "driver.hpp"
}

%define api.token.prefix {TOK_}
%token
  END  0  "end of file"
  SEMICOLON  ";"
  COMMA      ","
  MINUS      "-"
  PLUS       "+"
  STAR       "*"
  SLASH      "/"
  LPAREN     "("
  RPAREN     ")"
  RBRACE     "}"
  LBRACE     "{"
  QMARK      "?"
  COLON      ":"
  ASSIGN     "="
  LT         "<"
  EQ         "=="
  EXTERN     "extern"
  GLOBAL     "global"
  DEF        "def"
  VAR        "var"
  IF         "if"
  FOR        "for"
  INCREMENT  "++"
  DECREMENT  "--"
;

%token <std::string> IDENTIFIER "id"
%token <double> NUMBER "number"
%type <ExprAST*> exp idexp initexp
%type <std::vector<ExprAST*>> optexp explist
%type <RootAST*> program top init stmt
%type <std::vector<RootAST *>> stmts
%type <FunctionAST*> definition
%type <PrototypeAST*> external
%type <PrototypeAST*> proto
%type <std::vector<std::string>> idseq
%type <GlobalVarAST *> globalvar
%type <BlockAST *> block
%type <IfExprAST *> expif
%type <ConditionalExprAST *> condexp
%type <AssignmentAST *> assignment
%type <VarBindingAST *> binding
%type <std::vector<VarBindingAST *> > vardefs
%type <IfStatementAST *> ifstmt
%type <ForStatementAST *> forstmt
%%
%start startsymb;

startsymb:
program                 { drv.root = $1; }

program:
  %empty                { $$ = new SeqAST(nullptr,nullptr); }
|  top ";" program      { $$ = new SeqAST($1,$3); };

top:
%empty                  { $$ = nullptr; }
| definition            { $$ = $1; }
| external              { $$ = $1; }
| globalvar             { $$ = $1; }

definition:
  "def" proto block       { $$ = new FunctionAST($2,$3); $2->noemit(); };

external:
  "extern" proto        { $$ = $2; };

proto:
  "id" "(" idseq ")"    { $$ = new PrototypeAST($1,$3);  };

globalvar:
  "global" "id"         { $$ = new GlobalVarAST($2); }

idseq:
  %empty                { std::vector<std::string> args;
                         $$ = args; }
| "id" idseq            { $2.insert($2.begin(),$1); $$ = $2; };

%left ":";
%left "<" "==";
%left "+" "-";
%left "*" "/";

stmts:
  stmt                  { 
                          std::vector<RootAST *> statements;
                          statements.push_back($1);
                          $$ = statements;
                        }
| stmt ";" stmts        { $3.push_back($1); $$ = $3; }

stmt:
  assignment            { $$ = $1; }
| block                 { $$ = $1; }
| ifstmt                { $$ = $1; }
| forstmt               { $$ = $1; }
| exp                   { $$ = $1; }

ifstmt:
  "if" "(" condexp ")" stmt                 { $$ = new IfStatementAST($3, $5); }
| "if" "(" condexp ")" stmt "else" stmt     { $$ = new IfStatementAST($3, $5, $7); }

forstmt:
  "for" "(" init ";" condexp ";" assignment ")" stmt  { $$ = new ForStatementAST($3, $5, $7, $9); }

init:
  binding               { $$ = $1; }
| assignment            { $$ = $1; }

assignment:
  "id" "=" exp          { $$ = new AssignmentAST($1, $3); }
| "++" "id"             { $$ = new UnaryOperatorBaseAST($2, "+", -1); }
| "--" "id"             { $$ = new UnaryOperatorBaseAST($2, "-", -1); }
| "id" "++"             { $$ = new UnaryOperatorBaseAST($1, "+", 1); }
| "id" "--"             { $$ = new UnaryOperatorBaseAST($1, "-", 1); }

block:
  "{" stmts "}"               { $$ = new BlockAST($2); }
| "{" vardefs ";" stmts "}"   { $$ = new BlockAST($2, $4); }

vardefs:
  binding               { std::vector<VarBindingAST *> bindings; bindings.push_back($1); $$ = bindings; }
| vardefs ";" binding   { $1.push_back($3); $$ = $1; }

binding:
  "var" "id" initexp    { $$ = new VarBindingAST($2, $3); }

exp:
  exp "+" exp           { $$ = new BinaryExprAST('+',$1,$3); }
| exp "-" exp           { $$ = new BinaryExprAST('-',$1,$3); }
| exp "*" exp           { $$ = new BinaryExprAST('*',$1,$3); }
| exp "/" exp           { $$ = new BinaryExprAST('/',$1,$3); }
| idexp                 { $$ = $1; }
| "(" exp ")"           { $$ = $2; }
| "number"              { $$ = new NumberExprAST($1); };
| expif                 { $$ = $1; }

initexp:
  %empty                { $$ = nullptr; }
| "=" exp               { $$ = $2; }

expif:
  condexp "?" exp ":" exp { $$ = new IfExprAST($1, $3, $5); }

condexp:
  exp "<" exp           { $$ = new ConditionalExprAST('<', $1, $3); }
| exp "==" exp          { $$ = new ConditionalExprAST('=', $1, $3); }

idexp:
  "id"                  { $$ = new VariableExprAST($1); }
| "id" "(" optexp ")"   { $$ = new CallExprAST($1,$3); };

optexp:
  %empty                { std::vector<ExprAST*> args;
			 $$ = args; }
| explist               { $$ = $1; };

explist:
  exp                   {
                          std::vector<ExprAST*> args;
                          args.push_back($1);
                          $$ = args;
                        }
| exp "," explist       { $3.insert($3.begin(), $1); $$ = $3; };
 
%%

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}
