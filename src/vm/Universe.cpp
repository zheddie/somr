/*
 *
 *
Copyright (c) 2007 Michael Haupt, Tobias Pape, Arne Bergmann
Software Architecture Group, Hasso Plattner Institute, Potsdam, Germany
http://www.hpi.uni-potsdam.de/swa/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
  */


#include <sstream> 
#include <string.h>
#include <stdlib.h>

#include "Universe.h"
#include "Shell.h"

#include "VMSymbol.h"
#include "VMObject.h"
#include "VMMethod.h"
#include "VMClass.h"
#include "VMFrame.h"
#include "VMArray.h"
#include "VMBlock.h"
#include "VMDouble.h"
#include "VMInteger.h"
#include "VMString.h"
#include "VMBigInteger.h"
#include "VMEvaluationPrimitive.h"
#include "Symboltable.h"

#include "../interpreter/bytecodes.h"

#include "../compiler/Disassembler.h"
#include "../compiler/SourcecodeCompiler.h"


#include "omrport.h"
#include "ModronAssertions.h"
/* OMR Imports */
#include "AllocateDescription.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationLanguageInterfaceImpl.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrgcstartup.hpp"
#include "omrvm.h"
#include "StartupManagerImpl.hpp"
#include "omrExampleVM.hpp"



// Here we go:

short dumpBytecodes;
short gcVerbosity;




Universe* Universe::theUniverse = NULL;

uint32_t globalGcCount;
uint64_t globalObjectsAllocated;
pVMObject nilObject;
pVMObject trueObject;
pVMObject falseObject;
      
pVMClass objectClass;
pVMClass classClass;
pVMClass metaClassClass;
  
pVMClass nilClass;
pVMClass integerClass;
pVMClass bigIntegerClass;
pVMClass arrayClass;
pVMClass methodClass;
pVMClass symbolClass;
pVMClass frameClass;
pVMClass primitiveClass;
pVMClass stringClass;
pVMClass systemClass;
pVMClass blockClass;
pVMClass doubleClass;


/* Start up */
OMR_VM_Example exampleVM;
OMR_VMThread *omrVMThread = NULL;
omrthread_t self = NULL;



//Singleton accessor
Universe* Universe::GetUniverse() {
    if (!theUniverse) {
        ErrorExit("Trying to access uninitialized Universe, exiting.");
    }
	return theUniverse;
}


Universe* Universe::operator->() {
    if (!theUniverse) {
        ErrorExit("Trying to access uninitialized Universe, exiting.");
    }
	return theUniverse;
}


void Universe::Start(int argc, char** argv) {
	theUniverse = new Universe();
    theUniverse->initialize(argc, argv);
}


void Universe::Quit(int err) {
//	
    if (theUniverse) delete(theUniverse);
    /* OMR. Shut down */
    int rc = 0;
	/* Shut down */

	/* Free object hash table */
	hashTableForEachDo(exampleVM.objectTable, objectTableFreeFn, &exampleVM);
	hashTableFree(exampleVM.objectTable);
	exampleVM.objectTable = NULL;

	/* Free root hash table */
	hashTableFree(exampleVM.rootTable);
	exampleVM.rootTable = NULL;

	if (NULL != exampleVM._vmAccessMutex) {
		omrthread_rwmutex_destroy(exampleVM._vmAccessMutex);
		exampleVM._vmAccessMutex = NULL;
	}

	/* Shut down VM
	 * This destroys the port library and the omrthread library.
	 * Don't use any port library or omrthread functions after this.
	 *
	 * (This also shuts down trace functionality, so the trace assertion
	 * macros might not work after this.)
	 */
	rc = OMR_Shutdown_VM(exampleVM._omrVM, omrVMThread);
	/* Can not assert the value of rc since the portlibrary and trace engine have been shutdown */

//    	
    exit(err);
}


void Universe::ErrorExit( const char* err) {
    cout << "Runtime error: " << err << endl;
    Quit(ERR_FAIL);
}

vector<StdString> Universe::handleArguments( int argc, char** argv ) {

    vector<StdString> vmArgs = vector<StdString>();
    dumpBytecodes = 0;
    gcVerbosity = 0;
    for (int i = 1; i < argc ; ++i) {
        
        if (strncmp(argv[i], "-cp", 3) == 0) {
            if ((argc == i + 1) || classPath.size() > 0)
                printUsageAndExit(argv[0]);
            setupClassPath(StdString(argv[++i]));
        } else if (strncmp(argv[i], "-d", 2) == 0) {
            ++dumpBytecodes;
        } else if (strncmp(argv[i], "-g", 2) == 0) {
            ++gcVerbosity;
        } else if (argv[i][0] == '-' && argv[i][1] == 'H') {
            int heap_size = atoi(argv[i] + 2);
            heapSize = heap_size;
        } else if ((strncmp(argv[i], "-h", 2) == 0) ||
            (strncmp(argv[i], "--help", 6) == 0)) {
                printUsageAndExit(argv[0]);
        } else {
            vector<StdString> extPathTokens = vector<StdString>(2);
            StdString tmpString = StdString(argv[i]);
            if (this->getClassPathExt(extPathTokens, tmpString) ==
                                        ERR_SUCCESS) {
                this->addClassPath(extPathTokens[0]);
            }
            //Different from CSOM!!!:
            //In CSOM there is an else, where the original filename is pushed into the vm_args.
            //But unlike the class name in extPathTokens (extPathTokens[1]) that could
            //still have the .som suffix though.
            //So in SOM++ getClassPathExt will strip the suffix and add it to extPathTokens
            //even if there is no new class path present. So we can in any case do the following:
            vmArgs.push_back(extPathTokens[1]);
        }
    }
    addClassPath(StdString("."));

    return vmArgs;
}

int Universe::getClassPathExt(vector<StdString>& tokens,const StdString& arg ) const {
#define EXT_TOKENS 2
    int result = ERR_SUCCESS;
    int fpIndex = arg.find_last_of(fileSeparator);
    int ssepIndex = arg.find(".som");

    if (fpIndex == StdString::npos) { //no new path
        //different from CSOM (see also HandleArguments):
        //we still want to strip the suffix from the filename, so
        //we set the start to -1, in order to start the substring
        //from character 0. npos is -1 too, but this is to make sure
        fpIndex = -1;
        //instead of returning here directly, we have to remember that
        //there is no new class path and return it later
        result = ERR_FAIL;
    } else tokens[0] = arg.substr(0, fpIndex);
    
    //adding filename (minus ".som" if present) to second slot
    ssepIndex = ( (ssepIndex != StdString::npos) && (ssepIndex > fpIndex)) ?
                 (ssepIndex - 1) :
                 arg.length();
    tokens[1] = arg.substr(fpIndex + 1, ssepIndex - (fpIndex));
    return result;
}


int Universe::setupClassPath( const StdString& cp ) {
    try {
        std::stringstream ss ( cp );
        StdString token;

        int i = 0;
        while( getline(ss, token, pathSeparator) ) {
            classPath.push_back(token);
            ++i;
        }

        return ERR_SUCCESS;
    } catch(std::exception e){ 
        return ERR_FAIL;
    }
}


int Universe::addClassPath( const StdString& cp ) {
    classPath.push_back(cp);
    return ERR_SUCCESS;
}


void Universe::printUsageAndExit( char* executable ) const {
    cout << "Usage: " << executable << " [-options] [args...]" << endl << endl;
    cout << "where options include:" << endl;
    cout << "    -cp <directories separated by " << pathSeparator 
         << ">" << endl;
    cout << "        set search path for application classes" << endl;
    cout << "    -d  enable disassembling (twice for tracing)" << endl;
    cout << "    -g  enable garbage collection details:" << endl <<
                    "        1x - print statistics when VM shuts down" << endl <<
                    "        2x - print statistics upon each collection" << endl <<
                    "        3x - print statistics and dump _HEAP upon each "  << endl <<
                    "collection" << endl;
    cout << "    -Hx set the _HEAP size to x MB (default: 1 MB)" << endl;
    cout << "    -h  show this help" << endl;

    Quit(ERR_SUCCESS);
}


Universe::Universe(){
	this->compiler = NULL;
	this->symboltable = NULL;
	this->interpreter = NULL;
};


void Universe::initialize(int _argc, char** _argv) {
//Try OMR firstly.
//	 
	exampleVM._omrVM = NULL;
	exampleVM.rootTable = NULL;
	exampleVM.objectTable = NULL;
	globalGcCount = 0;
	/* Initialize the VM */
	/* Initialize the VM */
	omr_error_t rc = OMR_Initialize_VM(&exampleVM._omrVM, &omrVMThread, &exampleVM, NULL);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Set up the vm access mutex */
	intptr_t rw_rc = omrthread_rwmutex_init(&exampleVM._vmAccessMutex, 0, "VM exclusive access");
	Assert_MM_true(J9THREAD_RWMUTEX_OK == rw_rc);

	/* Initialize root table */
	exampleVM.rootTable = hashTableNew(
			exampleVM._omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(RootEntry), 0, 0, OMRMEM_CATEGORY_MM,
			rootTableHashFn, rootTableHashEqualFn, NULL, NULL);

	/* Initialize root table */
	exampleVM.objectTable = hashTableNew(
			exampleVM._omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(ObjectEntry), 0, 0, OMRMEM_CATEGORY_MM,
			objectTableHashFn, objectTableHashEqualFn, NULL, NULL);

//	OMRPORT_ACCESS_FROM_OMRVM(exampleVM._omrVM);
//	omrtty_printf("VM/GC INITIALIZED\n");


//OMR finished.
    heapSize = 2*1024*1024;

    vector<StdString> argv = this->handleArguments(_argc, _argv);
//    
    Heap::InitializeHeap(heapSize,&exampleVM);
    heap = _HEAP;
//    
    symboltable = new Symboltable();
    compiler = new SourcecodeCompiler();
    interpreter = new Interpreter();
//    
    InitializeGlobals();
//    
    pVMObject systemObject = NewInstance(systemClass);

    this->SetGlobal(SymbolForChars("nil"), nilObject);
    this->SetGlobal(SymbolForChars("true"), trueObject);
    this->SetGlobal(SymbolForChars("false"), falseObject);
//zg . We need to keep the True and False class into the roots
    this->SetGlobal(SymbolForChars("True"), (pVMObject)trueObject->GetClass());
    this->SetGlobal(SymbolForChars("False"), (pVMObject)falseObject->GetClass());

    this->SetGlobal(SymbolForChars("system"), systemObject);
    this->SetGlobal(SymbolForChars("System"), systemClass);
    this->SetGlobal(SymbolForChars("Block"), blockClass);
//    
    pVMMethod bootstrapMethod = NewMethod(SymbolForChars("bootstrap"), 1, 0);
    bootstrapMethod->SetBytecode(0, BC_HALT);
    bootstrapMethod->SetNumberOfLocals(0);

    bootstrapMethod->SetMaximumNumberOfStackElements(2);
    bootstrapMethod->SetHolder(systemClass);
    
    if (argv.size() == 0) {
        Shell* shell = new Shell(bootstrapMethod);
        shell->Start();
        return;
    }

    /* only trace bootstrap if the number of cmd-line "-d"s is > 2 */
    short trace = 2 - dumpBytecodes;
    if(!(trace > 0)) dumpBytecodes = 1;

    pVMArray argumentsArray = _UNIVERSE->NewArrayFromArgv(argv);
    
    pVMFrame bootstrapFrame = interpreter->PushNewFrame(bootstrapMethod);
    bootstrapFrame->Push(systemObject);
    bootstrapFrame->Push((pVMObject)argumentsArray);

    pVMInvokable initialize = 
        dynamic_cast<pVMInvokable>(systemClass->LookupInvokable(this->SymbolForChars("initialize:")));
    (*initialize)(bootstrapFrame);
    
    // reset "-d" indicator
    if(!(trace>0)) dumpBytecodes = 2 - trace;
//    
    cout <<"Starting the application....."<<endl<<endl;

    interpreter->Start();

//    
}


Universe::~Universe() {
    if (interpreter) 
        delete(interpreter);
    if (compiler) 
        delete(compiler);
    if (symboltable) 
        delete(symboltable);

	// check done inside
    Heap::DestroyHeap();
}

void Universe::InitializeGlobals() {
    //
    //allocate nil object
    //
	 
    nilObject = new (_HEAP) VMObject;
//    
    nilObject->SetField(0, nilObject);
//    
    //this->SetGlobal(SymbolForChars("nil"),(pVMObject)nilObject);		//Set the nilObject into the root table.
    metaClassClass = NewMetaclassClass();
//    
    objectClass     = NewSystemClass();
    nilClass        = NewSystemClass();
    classClass      = NewSystemClass();
    arrayClass      = NewSystemClass();
    symbolClass     = NewSystemClass();
    methodClass     = NewSystemClass();
    integerClass    = NewSystemClass();
    bigIntegerClass = NewSystemClass();
    frameClass      = NewSystemClass();
    primitiveClass  = NewSystemClass();
    stringClass     = NewSystemClass();
    doubleClass     = NewSystemClass();
//    
    nilObject->SetClass(nilClass);
//    
    InitializeSystemClass(objectClass, NULL, "Object");
//    
    InitializeSystemClass(classClass, objectClass, "Class");
    InitializeSystemClass(metaClassClass, classClass, "Metaclass");
    InitializeSystemClass(nilClass, objectClass, "Nil");
    InitializeSystemClass(arrayClass, objectClass, "Array");
    InitializeSystemClass(methodClass, arrayClass, "Method");
    InitializeSystemClass(symbolClass, objectClass, "Symbol");
    InitializeSystemClass(integerClass, objectClass, "Integer");
    InitializeSystemClass(bigIntegerClass, objectClass,
                                     "BigInteger");
    InitializeSystemClass(frameClass, arrayClass, "Frame");
    InitializeSystemClass(primitiveClass, objectClass,
                                     "Primitive");
    InitializeSystemClass(stringClass, objectClass, "String");
    InitializeSystemClass(doubleClass, objectClass, "Double");
//    
    LoadSystemClass(objectClass);
//    
    LoadSystemClass(classClass);
//    
    LoadSystemClass(metaClassClass);
//    
    LoadSystemClass(nilClass);
    LoadSystemClass(arrayClass);
    LoadSystemClass(methodClass);
    LoadSystemClass(symbolClass);
    LoadSystemClass(integerClass);
    LoadSystemClass(bigIntegerClass);
    LoadSystemClass(frameClass);
    LoadSystemClass(primitiveClass);
    LoadSystemClass(stringClass);
    LoadSystemClass(doubleClass);
//    
    blockClass = LoadClass(_UNIVERSE->SymbolForChars("Block"));

    trueObject = NewInstance(_UNIVERSE->LoadClass(_UNIVERSE->SymbolForChars("True")));
    falseObject = NewInstance(_UNIVERSE->LoadClass(_UNIVERSE->SymbolForChars("False")));

    systemClass = LoadClass(_UNIVERSE->SymbolForChars("System"));
//    
}

void Universe::Assert( bool value) const {
    if (!value)  {
        cout << "Assertion failed" << endl;
    }

}


pVMClass Universe::GetBlockClass() const {
    return blockClass;
}


pVMClass Universe::GetBlockClassWithArgs( int numberOfArguments) {
    this->Assert(numberOfArguments < 10);

    ostringstream Str;
    Str << "Block" << numberOfArguments ;
    StdString blockName(Str.str());
    pVMSymbol name = SymbolFor(blockName);

    if (HasGlobal(name))
        return (pVMClass)GetGlobal(name);

    pVMClass result = LoadClassBasic(name, NULL);

    result->AddInstancePrimitive(new (_HEAP) VMEvaluationPrimitive(numberOfArguments) );

    SetGlobal(name, (pVMObject) result);

    return result;
}



pVMObject Universe::GetGlobal( pVMSymbol name) {
//    if (HasGlobal(name))
//        return (pVMObject)globals[name];
//
//    return NULL;
	//Try to find them in the rootTable for OMR
	return HasGlobal(name);
}
//
//RootEntry rEntry = {name->GetChars(),(omrobjectptr_t)val};
//RootEntry *entryInTable = (RootEntry *)hashTableAdd(exampleVM.rootTable, &rEntry);
//if (NULL == entryInTable) {
//	OMRPORT_ACCESS_FROM_OMRVM(exampleVM._omrVM);
//	omrtty_printf("failed to add new root to root table!\n");
//}
///* update entry if it already exists in table */
//entryInTable->rootPtr = (omrobjectptr_t)val;


pVMObject  Universe::HasGlobal( pVMSymbol name) {
//    if (globals[name] != NULL) return true;
//    else return false;
	//Try to find them in the root table for OMR.
	J9HashTableState state;
	RootEntry *rEntry = NULL;
	rEntry = (RootEntry *)hashTableStartDo(exampleVM.rootTable, &state);
	while (rEntry != NULL) {
		//_markingScheme->markObject(env, rEntry->rootPtr);
		if(!strcmp(rEntry->name,name->GetChars())){
			return (pVMObject)rEntry->rootPtr;
		}
		rEntry = (RootEntry *)hashTableNextDo(&state);
	}
	return NULL;
}


void Universe::InitializeSystemClass( pVMClass systemClass, 
                                     pVMClass superClass, const char* name) {
    StdString s_name(name);
//    
    if (superClass != NULL) {
//    	
        systemClass->SetSuperClass(superClass);
        pVMClass sysClassClass = systemClass->GetClass();
        pVMClass superClassClass = superClass->GetClass();
        sysClassClass->SetSuperClass(superClassClass);
//        
    } else {
//    	
        pVMClass sysClassClass = systemClass->GetClass();
        sysClassClass->SetSuperClass(classClass);
//        
    }
//    
    pVMClass sysClassClass = systemClass->GetClass();
//    
    systemClass->SetInstanceFields(NewArray(0));
    sysClassClass->SetInstanceFields(NewArray(0));
//    
    systemClass->SetInstanceInvokables(NewArray(0));
//    
    sysClassClass->SetInstanceInvokables(NewArray(0));
//    
    systemClass->SetName(SymbolFor(s_name));
    ostringstream Str;
    Str << s_name << " class";
    StdString classClassName(Str.str());
    sysClassClass->SetName(SymbolFor(classClassName));
// zg.Add both the system class and it's clazz class into the root table here is better than when "new" them.
// as we can easily get the name for them. One is something like Object, Nil, etc and clazz class's name as "Object class" and "Nil class".

    SetGlobal(sysClassClass->GetName(), (pVMObject)sysClassClass);
    SetGlobal(systemClass->GetName(), (pVMObject)systemClass);
//    

}


pVMClass Universe::LoadClass( pVMSymbol name) {
   if (HasGlobal(name))
       return dynamic_cast<pVMClass>(GetGlobal(name));

   pVMClass result = LoadClassBasic(name, NULL);

   if (!result) {
       cout << "can\'t load class " << name->GetStdString() << endl;
       Universe::Quit(ERR_FAIL);
   }

   if (result->HasPrimitives() || result->GetClass()->HasPrimitives())
       result->LoadPrimitives(classPath);
    
   return result;
}


pVMClass Universe::LoadClassBasic( pVMSymbol name, pVMClass systemClass) {
//	
    StdString s_name = name->GetStdString();
    //cout << s_name.c_str() << endl;
    pVMClass result;
//    
    for (vector<StdString>::iterator i = classPath.begin();
         i != classPath.end(); ++i) {
//    	
        result = compiler->CompileClass(*i, name->GetStdString(), systemClass);
//        
        if (result) {
            if (dumpBytecodes) {
                Disassembler::Dump(result->GetClass());
                Disassembler::Dump(result);
            }
//            
//            if( NULL == systemClass){
//            		//zg.Looks like a new class is loaded by name. Need to add it into the root table.
//            		SetGlobal(name,result);
//            }
            return result;
        }

    }
//    
    return NULL;
}


pVMClass Universe::LoadShellClass( StdString& stmt) {
    pVMClass result = compiler->CompileClassString(stmt, NULL);
     if(dumpBytecodes)
         Disassembler::Dump(result);
    return result;
}


void Universe::LoadSystemClass( pVMClass systemClass) {
//	  
    pVMClass result =
        LoadClassBasic(systemClass->GetName(), systemClass);

//    
    if (!result) {
    		StdString s = systemClass->GetName()->GetStdString();
        cout << "Can\'t load system class: " << s;
        Universe::Quit(ERR_FAIL);
    }
//    
    if (result->HasPrimitives() || result->GetClass()->HasPrimitives())
        result->LoadPrimitives(classPath);
//    
}


pVMArray Universe::NewArray( int size) const {
    int additionalBytes = size*sizeof(pVMObject);
    pVMArray result = new (_HEAP, additionalBytes) VMArray(size);
    result->SetClass(arrayClass);
    return result;
}


pVMArray Universe::NewArrayFromArgv( const vector<StdString>& argv) const {
    pVMArray result = NewArray(argv.size());
    int j = 0;
    for (vector<StdString>::const_iterator i = argv.begin();
         i != argv.end(); ++i) {
        (*result)[j] =  NewString(*i);
        ++j;
    }

    return result;
}


pVMArray Universe::NewArrayList(ExtendedList<pVMObject>& list ) const {
    int size = list.Size();
//    
    pVMArray result = NewArray(size);
//   
    if (result)  {
        for (int i = 0; i < size; ++i) {
            pVMObject elem = list.Get(i);
            
            (*result)[i] = elem;
        }
    }
    return result;
}


pVMBigInteger Universe::NewBigInteger( int64_t value) const {
    pVMBigInteger result = new (_HEAP) VMBigInteger(value);
    result->SetClass(bigIntegerClass);

    return result;
}


pVMBlock Universe::NewBlock( pVMMethod method, pVMFrame context, int arguments) {
    pVMBlock result = new (_HEAP) VMBlock;
    result->SetClass(this->GetBlockClassWithArgs(arguments));

    result->SetMethod(method);
    result->SetContext(context);
    //SetGlobal(method->GetSignature(),(pVMObject)result);
    return result;
}


pVMClass Universe::NewClass( pVMClass classOfClass)  {
    int numFields = classOfClass->GetNumberOfInstanceFields();
    pVMClass result;
    int additionalBytes = numFields * sizeof(pVMObject);
    if (numFields) result = new (_HEAP, additionalBytes) VMClass(numFields);
    else result = new (_HEAP) VMClass;

    result->SetClass(classOfClass);
    //SetGlobal(SymbolForChars("NewClass"),(pVMObject)result);
    return result;
}


pVMDouble Universe::NewDouble( double value) const {
    pVMDouble result = new (_HEAP) VMDouble(value);
    result->SetClass(doubleClass);
    return result;
}


pVMFrame Universe::NewFrame( pVMFrame previousFrame, pVMMethod method)  {
    int length = method->GetNumberOfArguments() +
                 method->GetNumberOfLocals()+
                 method->GetMaximumNumberOfStackElements(); 
   
    int additionalBytes = length * sizeof(pVMObject);
    pVMFrame result = new (_HEAP, additionalBytes) VMFrame(length,method,previousFrame);
    //printf("zg.Universe::NewFrame.cp0,result=%p\n",result);
    //result->SetClass(frameClass);

    //result->SetMethod(method);

//    if (previousFrame != NULL)
//        result->SetPreviousFrame(previousFrame);
//
//    result->ResetStackPointer();
//    result->SetBytecodeIndex(0);
    //zg.don't we need to set it as global?
    //SetGlobal(frameClass->GetName(),(pVMObject)result);
    return result;
}


pVMObject Universe::NewInstance( pVMClass  classOfInstance) const {
    //the number of fields for allocation. We have to calculate the clazz
    //field out of this, because it is already taken care of by VMObject
    int numOfFields = classOfInstance->GetNumberOfInstanceFields() - 1;
    //the additional space needed is calculated from the number of fields
    int additionalBytes = numOfFields * sizeof(pVMObject);
    pVMObject result = new (_HEAP, additionalBytes) VMObject(numOfFields);
    result->SetClass(classOfInstance);
    return result;
}

pVMInteger Universe::NewInteger( int32_t value) const {
    pVMInteger result = new (_HEAP) VMInteger(value);
    result->SetClass(integerClass);
    return result;
}

pVMClass Universe::NewMetaclassClass() const {
    pVMClass result = new (_HEAP) VMClass;
    result->SetClass(new (_HEAP) VMClass);

    pVMClass mclass = result->GetClass();
    mclass->SetClass(result);

    SetGlobal( SymbolForChars("MetaClass"),(pVMObject)result);
    SetGlobal(SymbolForChars("MetaClassM"),(pVMObject)mclass);

    return result;
}


pVMMethod Universe::NewMethod( pVMSymbol signature, 
                    size_t numberOfBytecodes, size_t numberOfConstants)  {
    //Method needs space for the bytecodes and the pointers to the constants
    int additionalBytes = numberOfBytecodes + 
                numberOfConstants*sizeof(pVMObject);
    pVMMethod result = new (_HEAP,additionalBytes) 
                VMMethod(numberOfBytecodes, numberOfConstants);
    result->SetClass(methodClass);

    result->SetSignature(signature);
    //zg.don't we need to set it as global?
    // this->SetGlobal(signature,(pVMObject)result);
    return result;
}

pVMString Universe::NewString( const StdString& str) const {
    return NewString(str.c_str());
}

pVMString Universe::NewString( const char* str) const {
    //string needs space for str.length characters plus one byte for '\0'
    int additionalBytes = strlen(str) + 1;
    pVMString result = new (_HEAP, additionalBytes) VMString(str);
    result->SetClass(stringClass);

    return result;
}

pVMSymbol Universe::NewSymbol( const StdString& str)  const{
    return NewSymbol(str.c_str());
}

pVMSymbol Universe::NewSymbol( const char* str ) const {
    //symbol needs space for str.length characters plus one byte for '\0'
    int additionalBytes = strlen(str) + 1;
    pVMSymbol result = new (_HEAP, additionalBytes) VMSymbol(str);
    result->SetClass(symbolClass);

    symboltable->insert(result);

    return result;
}


//pVMClass Universe::NewSystemClass()  {
//    pVMClass systemClass = new (_HEAP) VMClass();
//
//    systemClass->SetClass(new (_HEAP) VMClass());
//    pVMClass mclass = systemClass->GetClass();
//
//    mclass->SetClass(metaClassClass);
//    //this->SetGlobal(SymbolForChars("SystemClass"),(pVMObject)systemClass);
//    return systemClass;
//}
pVMClass Universe::NewSystemClass()  {
//	//printf("zg.NewSystemClass.cp0,sizeofnm=%d,nm=%s\n",strlen(nm),nm);
//	char * nmm = (char *) malloc(strlen(nm)+2);		//Do we need to +2!
//	memset((void *)nmm,0,strlen(nmm));
//	strcpy(nmm, nm);
//	nmm[strlen(nm)]='M';
//	//printf("zg.NewSystemClass.cp1,nm=%s,nmm=%s\n",nm,nmm);
    pVMClass systemClass = new (_HEAP) VMClass();

    systemClass->SetClass(new (_HEAP) VMClass());
    pVMClass mclass = systemClass->GetClass();

    mclass->SetClass(metaClassClass);
    //SetGlobal(SymbolForChars(nm),(pVMObject)systemClass);    //This object would go into the root as named in initializeSystemClass
    //SetGlobal(SymbolForChars(nm),(pVMObject)mclass);

    return systemClass;
}

pVMSymbol Universe::SymbolFor( const StdString& str) {
    return SymbolForChars(str.c_str());
    
}


pVMSymbol Universe::SymbolForChars( const char* str) const {
    pVMSymbol result = symboltable->lookup(str);
    
    return (result != NULL) ?
           result :
           NewSymbol(str);
}


void Universe::SetGlobal(pVMSymbol name, VMObject *val) const{

    //globals[name] = val;
	RootEntry *rEntry = (RootEntry *)malloc (sizeof(RootEntry));
	char * strname = (char *) malloc (strlen( name->GetChars()));
	strcpy(strname,name->GetChars());
	rEntry -> name =strname;
	rEntry-> rootPtr = (omrobjectptr_t)val;

	RootEntry *entryInTable = (RootEntry *)hashTableAdd(exampleVM.rootTable, rEntry);
	if (NULL == entryInTable) {
		OMRPORT_ACCESS_FROM_OMRVM(exampleVM._omrVM);
		omrtty_printf("failed to add new root to root table!\n");
	}
	/* update entry if it already exists in table */
	entryInTable->rootPtr = (omrobjectptr_t)val;

}

void Universe::FullGC() {
    heap->FullGC();
}

