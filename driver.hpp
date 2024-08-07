#ifndef DRIVER_HPP
#define DRIVER_HPP
/************************* IR related modules ******************************/
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
/**************** C++ modules and generic data types ***********************/
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <variant>

#include "parser.hpp"

using namespace llvm;

// Dichiarazione del prototipo yylex per Flex
// Flex va proprio a cercare YY_DECL perché
// deve espanderla (usando M4) nel punto appropriato
# define YY_DECL \
  yy::parser::symbol_type yylex (driver& drv)
// Per il parser è sufficiente una forward declaration
YY_DECL;

// Classe che organizza e gestisce il processo di compilazione
class driver
{
public:
  driver();
  std::map<std::string, AllocaInst *> NamedValues; // < Symbol table
            /**
             * Tabella associativa in cui ogni 
             * chiave x è una variabile e il cui corrispondente valore è un'istruzione 
             * che alloca uno spazio di memoria della dimensione necessaria per 
             * memorizzare un variabile del tipo di x (nel nostro caso solo double)
             */

  RootAST* root;      // A fine parsing "punta" alla radice dell'AST
  int parse (const std::string& f);
  std::string file;
  bool trace_parsing; // Abilita le tracce di debug el parser
  void scan_begin (); // Implementata nello scanner
  void scan_end ();   // Implementata nello scanner
  bool trace_scanning;// Abilita le tracce di debug nello scanner
  yy::location location; // Utillizata dallo scannar per localizzare i token
  void codegen();
};

typedef std::variant<std::string,double> lexval;
const lexval NONE = 0.0;

// Classe base dell'intera gerarchia di classi che rappresentano
// gli elementi del programma
class RootAST {
protected:
  static Function *currentFunction();

public:
  virtual ~RootAST() {};
  virtual lexval getLexVal() const {return NONE;};
  virtual Value *codegen(driver& drv) { return nullptr; };
};

// Classe che rappresenta la sequenza di statement
class SeqAST : public RootAST {
private:
  RootAST* first;
  RootAST* continuation;

public:
  SeqAST(RootAST* first, RootAST* continuation);
  Value *codegen(driver& drv) override;
};

/// ExprAST - Classe base per tutti i nodi espressione
class ExprAST : public RootAST {};

/// NumberExprAST - Classe per la rappresentazione di costanti numeriche
class NumberExprAST : public ExprAST {
private:
  double Val;

public:
  NumberExprAST(double Val);
  lexval getLexVal() const override;
  Value *codegen(driver& drv) override;
};

/// VariableExprAST - Classe per la rappresentazione di riferimenti a variabili
class VariableExprAST : public ExprAST {
protected:
  std::string Name;
  
public:
  VariableExprAST(const std::string &Name);
  lexval getLexVal() const override;
  Value *codegen(driver& drv) override;
};

/// BinaryExprAST - Classe per la rappresentazione di operatori binari
class BinaryExprAST : public ExprAST {
private:
  char Op;
  ExprAST* LHS;
  ExprAST* RHS;

public:
  BinaryExprAST(char Op, ExprAST* LHS, ExprAST* RHS);
  Value *codegen(driver& drv) override;
};

/// CallExprAST - Classe per la rappresentazione di chiamate di funzione
class CallExprAST : public ExprAST {
private:
  std::string Callee;
  std::vector<ExprAST*> Args;  // ASTs per la valutazione degli argomenti

public:
  CallExprAST(std::string Callee, std::vector<ExprAST*> Args);
  lexval getLexVal() const override;
  Value *codegen(driver& drv) override;
};

/// PrototypeAST - Classe per la rappresentazione dei prototipi di funzione
/// (nome, numero e nome dei parametri; in questo caso il tipo è implicito
/// perché unico)
class PrototypeAST : public RootAST {
private:
  std::string Name;
  std::vector<std::string> Args;
  bool emitcode;

public:
  PrototypeAST(std::string Name, std::vector<std::string> Args);
  const std::vector<std::string> &getArgs() const;
  lexval getLexVal() const override;
  Function *codegen(driver& drv) override;
  void noemit();
};

/// FunctionAST - Classe che rappresenta la definizione di una funzione
class FunctionAST : public RootAST {
private:
  PrototypeAST* Proto;
  ExprAST* Body;
  bool external;
  
public:
  FunctionAST(PrototypeAST* Proto, ExprAST* Body);
  Function *codegen(driver& drv) override;
};

class IfExprAST: public ExprAST {
  private:
  ExprAST *cond;   
  ExprAST *trueexp;
  ExprAST *falseexp;

  public:
  IfExprAST(ExprAST *cond, ExprAST *trueexp, ExprAST *falseexp);
  Value *codegen(driver& drv) override;
};

class BlockAST: public ExprAST {
  private:
  std::vector<VarBindingAST *> Bindings;
  std::vector<RootAST *> Statements;

  public:
  BlockAST(std::vector<RootAST *>);
  BlockAST(std::vector<VarBindingAST *>, std::vector<RootAST *>);
  Value *codegen(driver &drv) override;
};

class VarBindingAST: public RootAST {
  private:
  ExprAST *Val;

  protected:  //Inherited by subclasses
  std::string Name;

  public:
  VarBindingAST(std::string Name, ExprAST *Val);
  std::string &getName();
  AllocaInst *codegen(driver& drv) override;
};

class AssignmentAST: public ExprAST {
  protected:
  std::string Id;
  ExprAST *Val;

  public:
  AssignmentAST(std::string Id, ExprAST *Val);
  Value * codegen(driver &drv) override;

  protected:
  /**
   * Returns the associated variable from the local table or the global table.
   */
  virtual Value * getVariable(driver &drv);
};

class RelationalExprAST: public ExprAST {
  private:
  char kind;   // < Can be `<` or `=`
  ExprAST *leftoperand;
  ExprAST *rightoperand;

  public:
  RelationalExprAST(char kind, ExprAST *leftoperand, ExprAST *rightoperand);
  Value *codegen(driver& drv) override;
};

class GlobalVarAST: public RootAST {
  private:
  std::string Name;

  protected:
  virtual Type * getVariableType();

  public:
  GlobalVarAST(std::string Name);
  std::string &getName();
  Constant *codegen(driver& drv) override;
};

class IfStatementAST: public RootAST {
  private:
  ExprAST *cond;   
  RootAST *truestmt;
  RootAST *falsestmt;

  public:
  IfStatementAST(ExprAST *cond, RootAST *truestmt);
  IfStatementAST(ExprAST *cond, RootAST *truestmt, RootAST *falsestmt);
  Value *codegen(driver& drv) override;
};

class ForInitAST: public RootAST {
  private:
  RootAST *init;
  bool binding;

  public:
  ForInitAST(RootAST *init, bool binding);
  Value *codegen(driver& drv) override;
  bool isBinding();
  std::string getName();
};

class ForStatementAST: public RootAST {
  private:
  ForInitAST *init;
  ConditionalExprAST *cond;
  AssignmentAST *update;
  RootAST *body;

  public:
  ForStatementAST(ForInitAST *init, ConditionalExprAST *cond, AssignmentAST *update, RootAST *body);
  Value *codegen(driver& drv) override;
};

/**
 * This class represents a common base for prefix/postfix increment/decrement
 * operators, that performs an assignment.
 * 
 * The actual specific operator is a subclass constructed using mixins.
 */
class UnaryOperatorBaseAST: public AssignmentAST {
  private:
  char Op;
  int order;

  public:
  /**
   * @param Op operator, can be "+" or "-"
   * @param order operation order, can be 1 post or -1 pre
   */
  UnaryOperatorBaseAST(std::string Id, char Op, int order);
  //Value * codegen(driver &drv) final;
};

class ConditionalExprAST: public ExprAST {
  private:
  std::string kind;
  RelationalExprAST *LHS;
  ConditionalExprAST *RHS;

  public:
  ConditionalExprAST(std::string kind, RelationalExprAST *LHS, ConditionalExprAST *RHS);
  ConditionalExprAST(RelationalExprAST *LHS);
  ConditionalExprAST(std::string kind, ConditionalExprAST *RHS);
  Value *codegen(driver& drv) override;
};

/**
 * Declaration
 */
class ArrayBindingAST: public VarBindingAST {
  private:
  int Size;
  std::vector<ExprAST *> Init;

  public:
  ArrayBindingAST(std::string Name, int Size);
  ArrayBindingAST(std::string Name, int Size, std::vector<ExprAST *> Init);
  AllocaInst *codegen(driver& drv) override;

  private:
  AllocaInst * CreateEntryBlockAlloca();
};

/**
 * Expression
 */
class ArrayExprAST: public VariableExprAST {
  private:
  ExprAST *Offset;

  public:
  ArrayExprAST(std::string Name, ExprAST *Offset);
  Value *codegen(driver &drv) override;
};

class ArrayAssignmentAST: public AssignmentAST {
  private:
  ExprAST *Offset;

  public:
  ArrayAssignmentAST(std::string Id, ExprAST *Offset, ExprAST *Value);
  virtual Value *getVariable(driver &drv) override;
};

class GlobalArrayAST: public GlobalVarAST {
  private:
  int Size;

  protected:
  Type * getVariableType() override;

  public:
  GlobalArrayAST(std::string Name, int Size);
};

#endif // ! DRIVER_HH
