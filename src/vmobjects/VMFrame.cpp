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


#include "VMFrame.h"
#include "VMMethod.h"
#include "VMObject.h"
#include "VMInteger.h"
#include "VMClass.h"
#include "VMSymbol.h"

#include "../vm/Universe.h"

//when doesNotUnderstand or UnknownGlobal is sent, additional stack slots might
//be necessary, as these cases are not taken into account when the stack
//depth is calculated. In that case this method is called.
pVMFrame VMFrame::EmergencyFrameFrom( pVMFrame from, int extraLength ) {
    int length = from->GetNumberOfIndexableFields() + extraLength;
    int additionalBytes = length * sizeof(pVMObject);
    pVMFrame result = new (_HEAP, additionalBytes) VMFrame(length,from->GetMethod(),from->GetPreviousFrame(),true);
    
//    result->SetClass(from->GetClass()); 	///Should be frameClass, shouldn't it?
//    //copy arguments, locals and the stack
//    from->CopyIndexableFieldsTo(result);
//
//    //set Frame members
//    result->SetPreviousFrame(from->GetPreviousFrame());
//    result->SetMethod(from->GetMethod());
//    result->SetContext(from->GetContext());
//    result->stackPointer = from->GetStackPointer();
//    result->bytecodeIndex = from->bytecodeIndex;
//    result->localOffset = from->localOffset;

    return result;
}


const int VMFrame::VMFrameNumberOfFields = 6; 

VMFrame::VMFrame(int size,pVMMethod m,pVMFrame pvf,bool iscopy, int nof) : VMArray(size, nof + VMFrameNumberOfFields) {


	strcpy(objectType,"VMFrame");
	Interpreter * ip = _UNIVERSE->GetInterpreter();

	if(iscopy){
	    this->SetClass(pvf->GetClass()); 	///Should be frameClass, shouldn't it?
	    //copy arguments, locals and the stack
	    pvf->CopyIndexableFieldsTo(this);

	    //set Frame members
	    this->SetPreviousFrame(pvf->GetPreviousFrame());
	    this->SetMethod(pvf->GetMethod());
	    this->SetContext(pvf->GetContext());
	    this->stackPointer = pvf->GetStackPointer();
	    this->bytecodeIndex = pvf->bytecodeIndex;
	    this->localOffset = pvf->localOffset;

		ip->SetFrame(this);
	}else{
		SetMethod(m);
		SetClass(frameClass);
		if(pvf == NULL) { pvf = (pVMFrame) nilObject;}
		SetPreviousFrame(pvf);
		ip->SetFrame(this);
		//Move as much as possible before extra space requested.
		_HEAP->StartUninterruptableAllocation();
		localOffset = _UNIVERSE->NewInteger(0);
		bytecodeIndex = _UNIVERSE->NewInteger(0);
		stackPointer = _UNIVERSE->NewInteger(0);
		_HEAP->EndUninterruptableAllocation();
	    ResetStackPointer();
	    //SetBytecodeIndex(0);
	}
}

pVMMethod VMFrame::GetMethod() const {
  
    return this->method;
}

void      VMFrame::SetMethod(pVMMethod method) {
    this->method = method;
}

bool     VMFrame::HasPreviousFrame() const {
    return previousFrame != nilObject && previousFrame != NULL;
}




bool     VMFrame::HasContext() const {
    return context !=  nilObject && context !=  NULL;
}


pVMFrame VMFrame::GetContextLevel(int lvl) {
    pVMFrame current = this;
    while (lvl > 0) {
        current = current->GetContext();
        --lvl;
    }
    return current;
}


pVMFrame VMFrame::GetOuterContext() {
    pVMFrame current = this;
    while (current->HasContext()) {
        current = current->GetContext();
    }
    return current;
}



int VMFrame::RemainingStackSize() const {
    // - 1 because the stack pointer points at the top entry,
    // so the next entry would be put at stackPointer+1
    return this->GetNumberOfIndexableFields() - 
           stackPointer->GetEmbeddedInteger() - 1;
}

pVMObject VMFrame::Pop() {
    int32_t sp = this->stackPointer->GetEmbeddedInteger();
    if(sp<0) printf("SOMR.ERROR, sp=%d, which is too small\n",sp);
    this->stackPointer->SetEmbeddedInteger(sp-1);
    pVMObject po = (*this)[sp];
    (*this)[sp] = nilObject;
    return po;
}


void      VMFrame::Push(pVMObject obj) {
    int32_t sp = this->stackPointer->GetEmbeddedInteger() + 1;
    if(sp >= this->GetNumberOfIndexableFields()) printf("SOMR.ERROR, sp=%d, which is bigger than indexable fields index(%d)\n",sp,this->GetNumberOfIndexableFields()-1);
    this->stackPointer->SetEmbeddedInteger(sp);
    (*this)[sp] = obj; 
}


void VMFrame::PrintStack() const {

	//cout<<"Holder:"<< GetMethod()->GetHolder()->GetClass()->GetName()->GetChars() <<"/Method:" <<GetMethod()->GetSignature()->GetChars()<< endl;
    cout <<"SP: " << this->stackPointer->GetEmbeddedInteger() << endl;
   // for (int i = 0; i < this->GetNumberOfIndexableFields()+1; ++i) {
    for (int i = 0; i < this->GetNumberOfIndexableFields(); ++i) {
        pVMObject vmo = (*this)[i];
        cout << i << ": ";
        if (vmo == NULL) 
            cout << "NULL" << endl;
        if (vmo == nilObject) 
            cout << "NIL_OBJECT" << endl;
        if (vmo->GetClass() == NULL) 
            cout << "VMObject with Class == NULL" << endl;
        if (vmo->GetClass() == nilObject) 
            cout << "VMObject with Class == NIL_OBJECT" << endl;
        else 

            cout << "index: " << i<<"("<<vmo<<")" << " object:"
                 << vmo->GetClass()->GetName()->GetChars() << endl;
    }
}
void VMFrame::PrintAllFrameStack() const {
	VMFrame * pfrm =(VMFrame *) this;
	pVMMethod method =  pfrm->GetMethod();
	int index = 0;
	while(pfrm != NULL && pfrm !=nilObject){
		pVMClass holder = method->GetHolder();
		char * holderclass = holder->GetClass()->GetName()->GetChars();
		char * methodname = method->GetSignature()->GetChars();
		//cout<<"Frame:"<<index++<<"/Holder:"<< holderclass <<"/Method:" <<methodname<<
		cout <<"SP: " << pfrm->stackPointer->GetEmbeddedInteger() << endl;
	   // for (int i = 0; i < this->GetNumberOfIndexableFields()+1; ++i) {
		for (int i = 0; i < pfrm->GetNumberOfIndexableFields(); ++i) {
			pVMObject vmo = (*pfrm)[i];
			cout << i << ": ";
			if (vmo == NULL)
				cout << "NULL" << endl;
			if (vmo == nilObject)
				cout << "NIL_OBJECT" << endl;
			if (vmo->GetClass() == NULL)
				cout << "VMObject with Class == NULL" << endl;
			if (vmo->GetClass() == nilObject)
				cout << "VMObject with Class == NIL_OBJECT" << endl;
			else
				cout << "index: " << i<<"("<<vmo<<")" << " object:"
					 << vmo->GetClass()->GetName()->GetChars() << endl;
		}
		pfrm = pfrm->previousFrame;
	}
}

void VMFrame::PrintAllFrames() const {
	VMFrame * pfrm =(VMFrame *) this;
	pVMMethod method =  pfrm->GetMethod();
	int idx1 = 0;
	while(pfrm != NULL && pfrm !=nilObject){
		pVMClass holder = method->GetHolder();
		char * holderclass = holder->GetClass()->GetName()->GetChars();
		char * methodname = method->GetSignature()->GetChars();
		cout<<"Frame("<<idx1++<<")"<<pfrm<<"/Holder:"<< holderclass <<"/Method:(" <<methodname<<")"<<endl;
		//cout <<"SP: " << pfrm->stackPointer->GetEmbeddedInteger() << endl;
		cout<<"<<<<<<<<context<<<<<<<<"<<endl;
		pVMFrame cntx = pfrm->GetContext();
		int idx2 = 0;
		while(cntx!=NULL && cntx!=nilObject){
			pVMMethod cntxM = cntx->GetMethod();
			pVMClass holder = cntxM->GetHolder();
			char * holderclass = holder->GetClass()->GetName()->GetChars();
			char * methodname = cntxM->GetSignature()->GetChars();
			cout<<"cntxFrame("<<idx2++<<")"<<cntx<<"/Holder:"<< holderclass <<"/Method:" <<methodname<<endl;
			cntx = cntx->GetContext();
		}
		cout<<">>>>>>>>>>>>>>>>"<<endl;
	   // for (int i = 0; i < this->GetNumberOfIndexableFields()+1; ++i) {
//		for (int i = 0; i < pfrm->GetNumberOfIndexableFields(); ++i) {
//			pVMObject vmo = (*pfrm)[i];
//			cout << i << ": ";
//			if (vmo == NULL)
//				cout << "NULL" << endl;
//			if (vmo == nilObject)
//				cout << "NIL_OBJECT" << endl;
//			if (vmo->GetClass() == NULL)
//				cout << "VMObject with Class == NULL" << endl;
//			if (vmo->GetClass() == nilObject)
//				cout << "VMObject with Class == NIL_OBJECT" << endl;
//			else
//				cout << "index: " << i<<"("<<vmo<<")" << " object:"
//					 << vmo->GetClass()->GetName()->GetChars() << endl;
//		}
		pfrm = pfrm->GetPreviousFrame();
	}

}

void      VMFrame::ResetStackPointer() {
    // arguments are stored in front of local variables
    pVMMethod meth = this->GetMethod();
    size_t lo = meth->GetNumberOfArguments();
    this->localOffset->SetEmbeddedInteger(lo);
  
    // Set the stack pointer to its initial value thereby clearing the stack
    size_t numLocals = meth->GetNumberOfLocals();
    this->stackPointer->SetEmbeddedInteger(lo + numLocals - 1);
}


int       VMFrame::GetBytecodeIndex() const {
    return this->bytecodeIndex->GetEmbeddedInteger();
}


void      VMFrame::SetBytecodeIndex(int index) {
    this->bytecodeIndex->SetEmbeddedInteger(index);
}


pVMObject VMFrame::GetStackElement(int index) const {
	//printf("zg.VMFrame::GetStackElement.cp0,index=%d\n",index);
    int sp = this->stackPointer->GetEmbeddedInteger();
    //printf("zg.VMFrame::GetStackElement.cp1,sp=%d\n",sp);
    return (*this)[sp-index];
}


void      VMFrame::SetStackElement(int index, pVMObject obj) {
    int sp = this->stackPointer->GetEmbeddedInteger();
    (*this)[sp-index] = obj; 
}


pVMObject VMFrame::GetLocal(int index, int contextLevel) {
    pVMFrame context = this->GetContextLevel(contextLevel);
    int32_t lo = context->localOffset->GetEmbeddedInteger();
    return (*context)[lo+index];
}


void      VMFrame::SetLocal(int index, int contextLevel, pVMObject value) {
    pVMFrame context = this->GetContextLevel(contextLevel);
    size_t lo = context->localOffset->GetEmbeddedInteger();
    (*context)[lo+index] = value; 
}



pVMObject VMFrame::GetArgument(int index, int contextLevel) {
    // get the context
    pVMFrame context = this->GetContextLevel(contextLevel);
    return (*context)[index];
}


void      VMFrame::SetArgument(int index, int contextLevel, pVMObject value) {
    pVMFrame context = this->GetContextLevel(contextLevel);
    (*context)[index] = value; 
}


void      VMFrame::PrintStackTrace() const {
    //TODO
}
//zg. This function is no use anywere?
int       VMFrame::ArgumentStackIndex(int index) const {
    pVMMethod meth = this->GetMethod();
    return meth->GetNumberOfArguments() - index - 1;
}


void      VMFrame::CopyArgumentsFrom(pVMFrame frame) {
    // copy arguments from frame:
    // - arguments are at the top of the stack of frame.
    // - copy them into the argument area of the current frame
    pVMMethod meth = this->GetMethod();
    int num_args = meth->GetNumberOfArguments();
    for(int i=0; i < num_args; ++i) {
        pVMObject stackElem = frame->GetStackElement(num_args - 1 - i);
        (*this)[i] = stackElem;
    }
}


void VMFrame::MarkReferences() {
    if (gcfield) return;
     VMArray::MarkReferences();
}


