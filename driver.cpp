#include "driver.hpp"
#include "parser.hpp"

#include <iostream>
using namespace std;

// Generazione di un'istanza per ciascuna della classi LLVMContext,
// Module e IRBuilder. Nel caso di singolo modulo è sufficiente
LLVMContext *context = new LLVMContext;
Module *module = new Module("Kaleidoscope", *context);
IRBuilder<> *builder = new IRBuilder(*context);

Value *LogErrorV(const std::string Str) {
  outs() << Str << "\n";
  return nullptr;
}

/* Il codice seguente sulle prime non è semplice da comprendere.
   Esso definisce una utility (funzione C++) con due parametri:
   1) la rappresentazione di una funzione llvm IR, e
   2) il nome per un registro SSA
   La chiamata di questa utility restituisce un'istruzione IR che alloca un double
   in memoria e ne memorizza il puntatore in un registro SSA cui viene attribuito
   il nome passato come secondo parametro. L'istruzione verrà scritta all'inizio
   dell'entry block della funzione passata come primo parametro.
   Si ricordi che le istruzioni sono generate da un builder. Per non
   interferire con il builder globale, la generazione viene dunque effettuata
   con un builder temporaneo TmpB
*/
static AllocaInst *CreateEntryBlockAlloca(Function *fun, StringRef VarName) {
  IRBuilder<> TmpB(&fun->getEntryBlock(), fun->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(*context), nullptr, VarName);
}

Function * RootAST::currentFunction() {
  return builder->GetInsertBlock()->getParent();
}

static Constant *getConstantVoid() {
  return Constant::getNullValue(Type::getVoidTy(*context));
}

// Implementazione del costruttore della classe driver
driver::driver(): trace_parsing(false), trace_scanning(false) {};

// Implementazione del metodo parse
int driver::parse (const std::string &f) {
  file = f;                    // File con il programma
  location.initialize(&file);  // Inizializzazione dell'oggetto location
  scan_begin();                // Inizio scanning (ovvero apertura del file programma)
  yy::parser parser(*this);    // Istanziazione del parser
  parser.set_debug_level(trace_parsing); // Livello di debug del parsed
  int res = parser.parse();    // Chiamata dell'entry point del parser
  scan_end();                  // Fine scanning (ovvero chiusura del file programma)
  return res;
}

// Implementazione del metodo codegen, che è una "semplice" chiamata del 
// metodo omonimo presente nel nodo root (il puntatore root è stato scritto dal parser)
void driver::codegen() {
  root->codegen(*this);
};

/************************* Sequence tree **************************/
SeqAST::SeqAST(RootAST* first, RootAST* continuation):
  first(first), continuation(continuation) {};

// La generazione del codice per una sequenza è banale:
// mediante chiamate ricorsive viene generato il codice di first e 
// poi quello di continuation (con gli opportuni controlli di "esistenza")
Value *SeqAST::codegen(driver& drv) {
  if (first != nullptr) {
    Value *f = first->codegen(drv);
  } else {
    if (continuation == nullptr) return nullptr;
  }
  Value *c = continuation->codegen(drv);
  return nullptr;
};

/********************* Number Expression Tree *********************/
NumberExprAST::NumberExprAST(double Val): Val(Val) {};

lexval NumberExprAST::getLexVal() const {
  // Non utilizzata, Inserita per continuità con versione precedente
  lexval lval = Val;
  return lval;
};

// Non viene generata un'struzione; soltanto una costante LLVM IR
// corrispondente al valore float memorizzato nel nodo
// La costante verrà utilizzata in altra parte del processo di generazione
// Si noti che l'uso del contesto garantisce l'unicità della costanti 
Value *NumberExprAST::codegen(driver& drv) {  
  return ConstantFP::get(*context, APFloat(Val));
};

/******************** Variable Expression Tree ********************/
VariableExprAST::VariableExprAST(const std::string &Name): Name(Name) {};

lexval VariableExprAST::getLexVal() const {
  lexval lval = Name;
  return lval;
};

// NamedValues è una tabella che ad ogni variabile (che, in Kaleidoscope1.0, 
// può essere solo un parametro di funzione) associa non un valore bensì
// la rappresentazione di una funzione che alloca memoria e restituisce in un
// registro SSA il puntatore alla memoria allocata. Generare il codice corrispondente
// ad una varibile equivale dunque a recuperare il tipo della variabile 
// allocata e il nome del registro e generare una corrispondente istruzione di load
// Negli argomenti della CreateLoad ritroviamo quindi: (1) il tipo allocato, (2) il registro
// SSA in cui è stato messo il puntatore alla memoria allocata (si ricordi che A è
// l'istruzione ma è anche il registro, vista la corrispodenza 1-1 fra le due nozioni), (3)
// il nome del registro in cui verrà trasferito il valore dalla memoria
Value *VariableExprAST::codegen(driver& drv) {
  AllocaInst *A = drv.NamedValues[Name];
  if (A)
    return builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
  
  GlobalVariable *G = module->getGlobalVariable(Name);
  if (G)
    return builder->CreateLoad(G->getValueType(), G, Name.c_str());

  return LogErrorV("Undeclared variable " + Name);
}

/******************** Binary Expression Tree **********************/
BinaryExprAST::BinaryExprAST(char Op, ExprAST* LHS, ExprAST* RHS):
  Op(Op), LHS(LHS), RHS(RHS) {};

// La generazione del codice in questo caso è di facile comprensione.
// Vengono ricorsivamente generati il codice per il primo e quello per il secondo
// operando. Con i valori memorizzati in altrettanti registri SSA si
// costruisce l'istruzione utilizzando l'opportuno operatore
Value *BinaryExprAST::codegen(driver& drv) {
  Value *L = LHS->codegen(drv);
  Value *R = RHS->codegen(drv);
  if (!L || !R) 
     return nullptr;
  switch (Op) {
  case '+':
    return builder->CreateFAdd(L,R,"addres");
  case '-':
    return builder->CreateFSub(L,R,"subres");
  case '*':
    return builder->CreateFMul(L,R,"mulres");
  case '/':
    return builder->CreateFDiv(L,R,"addres");
  default:  
    std::cout << Op << std::endl;
    return LogErrorV("Operatore binario non supportato");
  }
};

/********************* Call Expression Tree ***********************/
/* Call Expression Tree */
CallExprAST::CallExprAST(std::string Callee, std::vector<ExprAST*> Args):
  Callee(Callee),  Args(std::move(Args)) {};

lexval CallExprAST::getLexVal() const {
  lexval lval = Callee;
  return lval;
};

Value* CallExprAST::codegen(driver& drv) {
  // La generazione del codice corrispondente ad una chiamata di funzione
  // inizia cercando nel modulo corrente (l'unico, nel nostro caso) una funzione
  // il cui nome coincide con il nome memorizzato nel nodo dell'AST
  // Se la funzione non viene trovata (e dunque non è stata precedentemente definita)
  // viene generato un errore
  Function *CalleeF = module->getFunction(Callee);
  if (!CalleeF)
     return LogErrorV("Funzione non definita");
  // Il secondo controllo è che la funzione recuperata abbia tanti parametri
  // quanti sono gi argomenti previsti nel nodo AST
  if (CalleeF->arg_size() != Args.size())
     return LogErrorV("Numero di argomenti non corretto");
  // Passato con successo anche il secondo controllo, viene predisposta
  // ricorsivamente la valutazione degli argomenti presenti nella chiamata 
  // (si ricordi che gli argomenti possono essere espressioni arbitarie)
  // I risultati delle valutazioni degli argomenti (registri SSA, come sempre)
  // vengono inseriti in un vettore, dove "se li aspetta" il metodo CreateCall
  // del builder, che viene chiamato subito dopo per la generazione dell'istruzione
  // IR di chiamata
  std::vector<Value *> ArgsV;
  for (auto arg : Args) {
     ArgsV.push_back(arg->codegen(drv));
     if (!ArgsV.back())
        return nullptr;
  }
  return builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

/************************* Prototype Tree *************************/
PrototypeAST::PrototypeAST(std::string Name, std::vector<std::string> Args):
  Name(Name), Args(std::move(Args)), emitcode(true) {};  //Di regola il codice viene emesso

lexval PrototypeAST::getLexVal() const {
   lexval lval = Name;
   return lval;	
};

const std::vector<std::string>& PrototypeAST::getArgs() const { 
   return Args;
};

// Previene la doppia emissione del codice. Si veda il commento più avanti.
void PrototypeAST::noemit() { 
   emitcode = false; 
};

Function *PrototypeAST::codegen(driver& drv) {
  // Costruisce una struttura, qui chiamata FT, che rappresenta il "tipo" di una
  // funzione. Con ciò si intende a sua volta una coppia composta dal tipo
  // del risultato (valore di ritorno) e da un vettore che contiene il tipo di tutti
  // i parametri. Si ricordi, tuttavia, che nel nostro caso l'unico tipo è double.
  
  // Prima definiamo il vettore (qui chiamato Doubles) con il tipo degli argomenti
  std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*context));
  // Quindi definiamo il tipo (FT) della funzione
  FunctionType *FT = FunctionType::get(Type::getDoubleTy(*context), Doubles, false);
  // Infine definiamo una funzione (al momento senza body) del tipo creato e con il nome
  // presente nel nodo AST. ExternalLinkage vuol dire che la funzione può avere
  // visibilità anche al di fuori del modulo
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, *module);

  // Ad ogni parametro della funzione F (che, è bene ricordare, è la rappresentazione 
  // llvm di una funzione, non è una funzione C++) attribuiamo ora il nome specificato dal
  // programmatore e presente nel nodo AST relativo al prototipo
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  /* Abbiamo completato la creazione del codice del prototipo.
     Il codice può quindi essere emesso, ma solo se esso corrisponde
     ad una dichiarazione extern. Se invece il prototipo fa parte
     della definizione "completa" di una funzione (prototipo+body) allora
     l'emissione viene fatta al momendo dell'emissione della funzione.
     In caso contrario nel codice si avrebbe sia una dichiarazione
     (come nel caso di funzione esterna) sia una definizione della stessa
     funzione.
  */
  if (emitcode) {
    F->print(errs());
    fprintf(stderr, "\n");
  };
  
  return F;
}

/************************* Function Tree **************************/
FunctionAST::FunctionAST(PrototypeAST* Proto, ExprAST* Body): Proto(Proto), Body(Body) {};

Function *FunctionAST::codegen(driver& drv) {
  //cout << "Defining " << get<string>(Proto->getLexVal()) << endl; TODO remove debug

  // Verifica che la funzione non sia già presente nel modulo, cioò che non
  // si tenti una "doppia definizione"
  Function *function = 
      module->getFunction(std::get<std::string>(Proto->getLexVal()));
  // Se la funzione non è già presente, si prova a definirla, innanzitutto
  // generando (ma non emettendo) il codice del prototipo
  if (!function)
    function = Proto->codegen(drv);
  // Se, per qualche ragione, la definizione "fallisce" si restituisce nullptr
  if (!function)
    return nullptr;  

  // Altrimenti si crea un blocco di base in cui iniziare a inserire il codice
  BasicBlock *BB = BasicBlock::Create(*context, "entry", function);
  builder->SetInsertPoint(BB);
 
  // Ora viene la parte "più delicata". Per ogni parametro formale della
  // funzione, nella symbol table si registra una coppia in cui la chiave
  // è il nome del parametro mentre il valore è un'istruzione alloca, generata
  // invocando l'utility CreateEntryBlockAlloca già commentata.
  // Vale comunque la pena ricordare: l'istruzione di allocazione riserva 
  // spazio in memoria (nel nostro caso per un double) e scrive l'indirizzo
  // in un registro SSA
  // Il builder crea poi un'istruzione che memorizza il valore del parametro x
  // (al momento contenuto nel registro SSA %x) nell'area di memoria allocata.
  // Si noti che il builder conosce il registro che contiene il puntatore all'area
  // perché esso è parte della rappresentazione C++ dell'istruzione di allocazione
  // (variabile Alloca) 
  
  for (auto &Arg : function->args()) {
    // Genera l'istruzione di allocazione per il parametro corrente
    AllocaInst *Alloca = CreateEntryBlockAlloca(function, Arg.getName());
    // Genera un'istruzione per la memorizzazione del parametro nell'area
    // di memoria allocata
    builder->CreateStore(&Arg, Alloca);
    // Registra gli argomenti nella symbol table per eventuale riferimento futuro
    drv.NamedValues[std::string(Arg.getName())] = Alloca;
  } 
  
  // Ora può essere generato il codice corssipondente al body (che potrà
  // fare riferimento alla symbol table)
  if (Value *RetVal = Body->codegen(drv)) {
    // Se la generazione termina senza errori, ciò che rimane da fare è
    // di generare l'istruzione return, che ("a tempo di esecuzione") prenderà
    // il valore lasciato nel registro RetVal 

    if (not RetVal->getType()->isVoidTy())
      builder->CreateRet(RetVal);

    // Effettua la validazione del codice e un controllo di consistenza
    verifyFunction(*function);
 
    // Emissione del codice su su stderr) 
    function->print(errs());
    fprintf(stderr, "\n");
    return function;
  }

  // Errore nella definizione. La funzione viene rimossa
  function->eraseFromParent();
  return nullptr;
};

IfExprAST::IfExprAST(ExprAST *cond, ExprAST *trueexp, ExprAST *falseexp) :
cond(cond), trueexp(trueexp), falseexp(falseexp) {}

Value * IfExprAST::codegen(driver& drv)
{
  // Valutiamo la condizione
  Value *condv = cond->codegen(drv); //deve restituire un booleano, quindi un i1
  if (not condv)
    return LogErrorV("Codegen for condexp returned nullptr");

  // Dove mandiamo l'istruzione di branch condizionato? Generiamo prima i blocchi.
  // Dobbiamo prima trovare il riferimento alla funzione in cui inserirlo.
  Function *fun = builder->GetInsertBlock()->getParent();   // < la funzione corrente
  BasicBlock *TrueBB = BasicBlock::Create(*context, "trueblock", fun);
  BasicBlock *FalseBB = BasicBlock::Create(*context, "falseblock");
  BasicBlock *MergeBB = BasicBlock::Create(*context, "mergeblock");

  builder->CreateCondBr(condv, TrueBB, FalseBB);

  // Posso cominciare a generare la parte true
  // Cambiamo blocco del builder.
  builder->SetInsertPoint(TrueBB);

  Value *TrueV = trueexp->codegen(drv);  // codegen chiama il builder e inserisce il codice
  if (not TrueV)
    return nullptr;
  
  TrueBB = builder->GetInsertBlock();
  builder->CreateBr(MergeBB);

  // Possiamo inserire il blocck false.
  fun->insert(fun->end(), FalseBB);  // inserisci il blocco alla fine della funzione
  builder->SetInsertPoint(FalseBB);

  Value *FalseV = falseexp->codegen(drv);
  if (not FalseV)
    return nullptr;

  // Come true, anche false potrebbe essersi ulteriormente suddiviso. Sarebbe inutile se i blocchi fossero
  // monolitici, ma non lo sono.
  FalseBB = builder->GetInsertBlock();
  builder->CreateBr(MergeBB);

  // Inseriamo il merge block
  fun->insert(fun->end(), MergeBB);
  builder->SetInsertPoint(MergeBB);

  // Riunione dei flussi. PHINode è un particolare value.
  PHINode *P = builder->CreatePHI(Type::getDoubleTy(*context), 2);  // il 2 sta per numero di coppie uguale al
                                                                    // numero di flussi che riunisce PHI
  P->addIncoming(TrueV, TrueBB);
  P->addIncoming(FalseV, FalseBB);

  return P;
}

/***** Block Expression Tree *****/

BlockAST::BlockAST(std::vector<RootAST *> Statements): Statements(std::move(Statements)) {}

BlockAST::BlockAST(std::vector<VarBindingAST *> Bindings, std::vector<RootAST *> Statements): Bindings(std::move(Bindings)), Statements(std::move(Statements)) {}

Value * BlockAST::codegen(driver &drv) {
  //  A block is made of both variable definitions (local to the block) and statements
  //  Bindings can shadow variables, thus they must be replaced before generating code for
  //  the statements.

  std::map<std::string, AllocaInst *> shadowed;

  for (auto bind: Bindings) {
    std::string const &name = bind->getName();
    AllocaInst *boundVal = bind->codegen(drv);
    if (not boundVal) {
      return LogErrorV("Invalid variable binding"); // invalid binding
    }

    auto alloc = drv.NamedValues[name];
    if (alloc)
      shadowed[name] = alloc;

    drv.NamedValues[name] = boundVal;
    //TODO: do we need to shadow a global variable? Local are always checked first...
  }

  Value *ret;
  for (auto stptr = Statements.rbegin(); stptr != Statements.rend(); stptr++) {
    auto &stmt = *stptr;
    ret = stmt->codegen(drv);
    if (not ret)
      return LogErrorV("Error in generating calls for block");
  }


  // Restore shadowed variables
  for (auto bind: Bindings) {
    drv.NamedValues.erase(bind->getName());
  }

  drv.NamedValues.merge(shadowed);

  return ret;
}


VarBindingAST::VarBindingAST(std::string Name, ExprAST *Val): Name(Name), Val(Val) {}

std::string & VarBindingAST::getName() {
  return Name;
}

AllocaInst * VarBindingAST::codegen(driver &drv) {
  Function *fun = builder->GetInsertBlock()->getParent();
  Value *ExpVal = Val->codegen(drv);
  AllocaInst *alloc = CreateEntryBlockAlloca(fun, Name);

  //cout << "Var binding of " << Name << endl;
  drv.NamedValues[Name] = alloc;

  if (not ExpVal or not alloc)
    return nullptr;

  builder->CreateStore(ExpVal, alloc);

  return alloc;
}

AssignmentAST::AssignmentAST(std::string Id, ExprAST *Val): Id(Id), Val(Val) {}

Value * AssignmentAST::getVariable(driver &drv) {
  //  Search variable in local table
  Value *ptr = drv.NamedValues[Id];
  if (ptr)
    return ptr;
  
  //  Resolve global table
  ptr = module->getGlobalVariable(Id);

  if (ptr)
    return ptr;

  return nullptr;
}

Value * AssignmentAST::codegen(driver &drv) {
  Value *rval = Val->codegen(drv);

  Value *ptr = getVariable(drv);

  if (not ptr)
    return LogErrorV("Variable not declared.");

  return builder->CreateStore(rval, ptr, false);
}

GlobalVarAST::GlobalVarAST(std::string Name): Name(Name) {}

Type * GlobalVarAST::getVariableType() {
  return Type::getDoubleTy(*context);
}

Constant * GlobalVarAST::codegen(driver &drv) {
  //  Find out if the variable is already defined
  if(module->getGlobalVariable(Name))
    return (GlobalVariable *)LogErrorV("Global variable already defined");

  auto contextDouble = getVariableType();
  GlobalVariable *var = new GlobalVariable(*module, contextDouble, false, GlobalValue::CommonLinkage, Constant::getNullValue(contextDouble), Name);

  var->print(errs()); // Output variable definition on stderr
  cerr << endl;

  return var;
}

RelationalExprAST::RelationalExprAST(char kind, ExprAST *leftoperand, ExprAST *rightoperand):
kind(kind), leftoperand(leftoperand), rightoperand(rightoperand) {}

Value * RelationalExprAST::codegen(driver& drv) {
  Value *lhsVal = leftoperand->codegen(drv);
  Value *rhsVal = rightoperand->codegen(drv);

  Value *ret;

  if (kind == '=') {
    ret = builder->CreateCmp(llvm::CmpInst::Predicate::FCMP_OEQ, lhsVal, rhsVal); // Ordered compare expects both sides to be valid numbers, not NaNs
  } else if (kind == '<') {
    ret = builder->CreateCmp(llvm::CmpInst::Predicate::FCMP_OLT, lhsVal, rhsVal);
  } else {
    return LogErrorV("Compare operand not supported");
  }

  return ret;
}

IfStatementAST::IfStatementAST(ExprAST *cond, RootAST *truestmt): cond(cond), truestmt(truestmt), falsestmt(nullptr) {}
IfStatementAST::IfStatementAST(ExprAST *cond, RootAST *truestmt, RootAST *falsestmt): cond(cond), truestmt(truestmt), falsestmt(falsestmt) {}

Value * IfStatementAST::codegen(driver& drv) {
  // Valutiamo la condizione
  Value *condv = cond->codegen(drv); //deve restituire un booleano, quindi un i1
  if (not condv)
    return LogErrorV("Codegen for condexp returned nullptr");

  Function *fun = builder->GetInsertBlock()->getParent();   // < la funzione corrente
  BasicBlock *TrueBB = BasicBlock::Create(*context, "trueblock", fun);
  BasicBlock *FalseBB = BasicBlock::Create(*context, "falseblock");
  BasicBlock *MergeBB = BasicBlock::Create(*context, "mergeblock");

  if (falsestmt) {
    builder->CreateCondBr(condv, TrueBB, FalseBB);

    builder->SetInsertPoint(TrueBB);
    Value *TrueV = truestmt->codegen(drv);  // codegen chiama il builder e inserisce il codice
    if (not TrueV)
      return nullptr;
    
    TrueBB = builder->GetInsertBlock();
    builder->CreateBr(MergeBB);

    // If an "else" statement is specified, create another branch
    fun->insert(fun->end(), FalseBB);  // inserisci il blocco alla fine della funzione
    builder->SetInsertPoint(FalseBB);

    Value *FalseV = falsestmt->codegen(drv);
    if (not FalseV)
      return nullptr;

    // Come true, anche false potrebbe essersi ulteriormente suddiviso. Sarebbe inutile se i blocchi fossero
    // monolitici, ma non lo sono.
    FalseBB = builder->GetInsertBlock();
    builder->CreateBr(MergeBB);

  } else {
    builder->CreateCondBr(condv, TrueBB, MergeBB);

    builder->SetInsertPoint(TrueBB);
    Value *TrueV = truestmt->codegen(drv);  // codegen chiama il builder e inserisce il codice
    if (not TrueV)
      return nullptr;
    
    TrueBB = builder->GetInsertBlock();
    builder->CreateBr(MergeBB);

  }

  // Inseriamo il merge block
  fun->insert(fun->end(), MergeBB);
  builder->SetInsertPoint(MergeBB);

  return getConstantVoid();
}

ForStatementAST::ForStatementAST(RootAST *init, ConditionalExprAST *cond, AssignmentAST *update, RootAST *stmt):
init(init), cond(cond), update(update), stmt(stmt) {}

Value * ForStatementAST::codegen(driver& drv) {
  Function *fun = builder->GetInsertBlock()->getParent();
  BasicBlock *forInit = BasicBlock::Create(*context, "forinit", fun);
  BasicBlock *condition = BasicBlock::Create(*context, "condition", fun);
  BasicBlock *body = BasicBlock::Create(*context, "body", fun);
  BasicBlock *exit = BasicBlock::Create(*context, "forexit", fun);

  //  Point to init from current BB
  builder->CreateBr(forInit);

  //  Init variable
  builder->SetInsertPoint(forInit);
  init->codegen(drv);
  builder->CreateBr(condition);

  //  Check condition
  builder->SetInsertPoint(condition);
  Value *condval = cond->codegen(drv);
  builder->CreateCondBr(condval, body, exit);

  builder->SetInsertPoint(body);
  stmt->codegen(drv);
  update->codegen(drv);
  builder->CreateBr(condition);

  builder->SetInsertPoint(exit);

  PHINode *P = builder->CreatePHI(Type::getDoubleTy(*context), 1);
  P->addIncoming(ConstantFP::getNullValue(Type::getDoubleTy(*context)), condition);

  return P;
}

UnaryOperatorBaseAST::UnaryOperatorBaseAST(std::string Id, char Op, int order):
Op(Op), order(order), AssignmentAST(Id, new BinaryExprAST(Op, new VariableExprAST(Id), new NumberExprAST(1))) {}

/**
 * TODO: fix
 * Value * UnaryOperatorBaseAST::codegen(driver &drv) {
  // Get value for variable
  Value *ptr = getVariable(drv);
  if (not ptr)
    return LogErrorV("Variable not declared.");

  Value *preopValue = Val->codegen(drv);
  Value *newValue;
  
  if (Op == '+') {
    newValue = builder->CreateFAdd(preopValue, ConstantFP::get(*context, APFloat(1.0)));
  } else if (Op == '-') {
    newValue = builder->CreateFSub(preopValue, ConstantFP::get(*context, APFloat(1.0)));
  } else {
    return LogErrorV(Op + " is an invalid operation");
  }

  return newValue;

  return builder->CreateStore(newValue, ptr);

  if (order < 0) {  //prefix
    return newValue;
  } else if (order > 0) { //postfix
    return preopValue;
  } else {
    return LogErrorV("Invalid order = 0");
  }
}*/


ConditionalExprAST::ConditionalExprAST(std::string kind, RelationalExprAST *LHS, ConditionalExprAST *RHS):
kind(kind), LHS(LHS), RHS(RHS) {}

ConditionalExprAST::ConditionalExprAST(RelationalExprAST *LHS): kind(""), LHS(LHS), RHS(nullptr) {}

ConditionalExprAST::ConditionalExprAST(std::string kind, ConditionalExprAST *RHS): kind(kind), LHS(nullptr), RHS(RHS) {}

Value * ConditionalExprAST::codegen(driver& drv) {
  if (kind == "") {
    return LHS->codegen(drv);
  } else if (kind == "not") {
    Value *cond = RHS->codegen(drv);
    return builder->CreateNeg(cond);
  }
  
  if (kind == "and") {
    Value *LHSCond = LHS->codegen(drv);
    Value *RHSCond = RHS->codegen(drv);
    return builder->CreateAnd(LHSCond, RHSCond);
  } else if (kind == "or") {
    Value *LHSCond = LHS->codegen(drv);
    Value *RHSCond = RHS->codegen(drv);
    return builder->CreateOr(LHSCond, RHSCond);
  } else {
    return LogErrorV("Invalid conditonal operation kind: " + kind);
  }
}



ArrayBindingAST::ArrayBindingAST(std::string Name, int Size): ArrayBindingAST(Name, Size, {}) {}

ArrayBindingAST::ArrayBindingAST(std::string Name, int Size, std::vector<ExprAST *> Init): Size(Size), Init(Init), VarBindingAST(Name, nullptr) {}

AllocaInst * ArrayBindingAST::CreateEntryBlockAlloca() {
  Function *fun = currentFunction();

  IRBuilder<> TmpBlock(&fun->getEntryBlock(), fun->getEntryBlock().begin());

  ArrayType *type = ArrayType::get(Type::getDoubleTy(*context), Size);
  return TmpBlock.CreateAlloca(type, nullptr, Name);
}

AllocaInst * ArrayBindingAST::codegen(driver& drv) {
  if (Init.size() != Size)
    return (AllocaInst *)LogErrorV("Initialization array for " + Name + " is not the same size as binding array");

  AllocaInst *alloc = CreateEntryBlockAlloca();  //< Base ptr for array
  if (not alloc)
    return (AllocaInst *)LogErrorV("Can't create stack array " + Name);

  ArrayType *type = ArrayType::get(Type::getDoubleTy(*context), Size);
  std:vector<Value *> initValues = {};

  for (auto initParam: Init) {
    initValues.push_back(initParam->codegen(drv));
  }

  Value *InitStore;
  for (int i=0; i<initValues.size(); i++) {
    Value *Index = llvm::ConstantInt::get(builder->getInt32Ty(), i);
    Value *ElementPtr = builder->CreateInBoundsGEP(type, alloc, {builder->getInt32(0), Index});

    InitStore = builder->CreateStore(initValues[i], ElementPtr);
  }

  drv.NamedValues[Name] = alloc;

  return alloc;
}


ArrayExprAST::ArrayExprAST(std::string Name, ExprAST *Offset): Offset(Offset), VariableExprAST(Name) {}

Value * ArrayExprAST::codegen(driver &drv) {
  AllocaInst *A = drv.NamedValues[Name];

  //  Compute the offset value
  Value *offsetFloat = Offset->codegen(drv);

  //  Cast to integer
  Value *Index = builder->CreateFPToUI(offsetFloat, builder->getInt32Ty());

  if (A) {
    if (not A->isArrayAllocation())
      return LogErrorV(Name + " is not an array type");

    if (ArrayType *ArrType = dyn_cast<ArrayType>(A->getAllocatedType()); ArrType and not ArrType->getElementType()->isDoubleTy())
      return LogErrorV(Name + " is not an array of doubles");
    
    Value *ElementPtr = builder->CreateInBoundsGEP(A->getAllocatedType(), A, {builder->getInt32(0), Index});
    return builder->CreateLoad(Type::getDoubleTy(*context), ElementPtr, Name.c_str());
  }

  if (GlobalVariable *G = module->getGlobalVariable(Name); G) {
    if (not G->getValueType()->isArrayTy())
      return LogErrorV("Global variable " + Name + " is not an array");

    if (ArrayType *ArrType = dyn_cast<ArrayType>(G->getValueType()); ArrType and not ArrType->getElementType()->isDoubleTy())
      return LogErrorV(Name + " is not an array of doubles");    

    Value *ElementPtr = builder->CreateInBoundsGEP(G->getValueType(), G, {builder->getInt32(0), Index});
    return builder->CreateLoad(Type::getDoubleTy(*context), ElementPtr, Name.c_str());
  }

  return LogErrorV("Undeclared array " + Name);
}

ArrayAssignmentAST::ArrayAssignmentAST(std::string Id, ExprAST *Offset, ExprAST *Value): AssignmentAST(Id, Value), Offset(Offset) {}

Value * ArrayAssignmentAST::getVariable(driver &drv) {
  Value *ptr = AssignmentAST::getVariable(drv);

  //  Compute the offset value
  Value *offsetFloat = Offset->codegen(drv);

  //  Cast to integer
  Value *Index = builder->CreateFPToUI(offsetFloat, builder->getInt32Ty());

  Value *ElementPtr;  //< Contains the element pointer of base+offset

  if (not ptr)
    return LogErrorV("Undeclared identifier " + Id);

  if (AllocaInst *basePtr = dyn_cast<AllocaInst>(ptr); basePtr) {
    if (not basePtr->isArrayAllocation())
      return LogErrorV(Id + " does not identify an array");

    ElementPtr = builder->CreateInBoundsGEP(basePtr->getAllocatedType(), basePtr, {builder->getInt32(0), Index});
  } else if (GlobalVariable *basePtr = dyn_cast<GlobalVariable>(ptr); basePtr) {
    if (not basePtr->getValueType()->isArrayTy())
      return LogErrorV("Global " + Id + "does not identify an array");

    ElementPtr = builder->CreateInBoundsGEP(basePtr->getValueType(), basePtr, {builder->getInt32(0), Index});
  }

  return ElementPtr;
}


GlobalArrayAST::GlobalArrayAST(std::string Name, int Size): GlobalVarAST(Name), Size(Size) {}

Type * GlobalArrayAST::getVariableType() {
  return ArrayType::get(Type::getDoubleTy(*context), Size);
}
