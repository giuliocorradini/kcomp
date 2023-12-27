%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.2"
%defines

%define api.token.constructor
%define api.location.file none
%define api.value.type variant
%define parse.assert

%code requires {
  # include <string>
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
  RCURBRACK  "}"
  LCURBRACK  "{"
  QSTMARK    "?"
  COLON      ":"
  EQUALS     "="
  COMPEQ     "=="
  EXTERN     "extern"
  DEF        "def"
  VAR        "var"
;

%token <std::string> IDENTIFIER "id"
%token <double> NUMBER "number"
%type <ExprAST*> exp
%type <ExprAST*> idexp
%type <std::vector<ExprAST*>> optexp
%type <std::vector<ExprAST*>> explist
%type <RootAST*> program
%type <RootAST*> top
%type <FunctionAST*> definition
%type <PrototypeAST*> external
%type <PrototypeAST*> proto
%type <std::vector<std::string>> idseq
%type <GlobalVarAST *> globalvar
%type <StatementAST *> stmt
%type <std::vector<StatementAST *> > stmts;
%type <AssignmentAST *> assignment;
%type <BlockAST *> block;
%type <std::vector<BindingAST *> > vardefs;
%type <BindingAST *> binding;
%type <IfExprAST *> expif;
%type <ConditionalExprAST *> condexp;
%type <ExprAST *> initexp;
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
                          std::vector<StatementAST *> statements;
                          statements.push_back($1);
                          $$ = statements;
                        }
| stmt ";" stmts        { $3.push_back($1); $$ = $3; }

stmt:
  assignment            { $$ = $1; }
| block                 { $$ = $1; }
| exp                   { $$ = $1; }

assignment:
  "id" "=" exp          { $$ = new AssignmentAST($1, $3); }

block:
  "{" stmts "}"               { $$ = new BlockAST(nullptr, $2); }
| "{" vardefs ";" stmts "}"   { $$ = new BlockAST($2, $4); }

vardefs:
  binding               { std::vector<BindingAST *> bindings; bindings.push_back($1); $$ = bindings; }
| vardefs ";" binding   { $1.push_back($3); $$ = $1; }

binding:
  "var" "id" initexp    { $$ = new BindingAST($2, $3); }

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
