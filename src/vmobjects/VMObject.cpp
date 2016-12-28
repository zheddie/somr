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


#include <typeinfo>
#include "VMObject.h"
#include "VMClass.h"
#include "VMSymbol.h"
#include "VMFrame.h"
#include "VMInvokable.h"


//clazz is the only field of VMObject so
const int VMObject::VMObjectNumberOfFields = 1; 

VMObject::VMObject( int numberOfFields ) {
    //this line would be needed if the VMObject** is used instead of the macro:
    //FIELDS = (pVMObject*)&clazz; 
    this->SetNumberOfFields(numberOfFields + VMObjectNumberOfFields);
    gcfield = 0; 

//    int32_t  low  =(int32_t) ((uintptr_t)this & 0xFFFF);
//    int32_t high = (int32_t) ((uintptr_t)this >>32);
//    hash = low+high;
	hash =(uintptr_t) this;
	this->SetClass(NULL);
	memset(objectType,0,OTLEN);
	strcpy(objectType,"VMObject");
	reserved_align  = 0;
	markablefieldindex = 0;
	addToObjectTable();
    //Object size is set by the heap
}

void VMObject::addToObjectTable(){
	//

	   char * name =(char *)malloc(20);
	   memset(name,0,20);
	  sprintf(name,"%9s%p",objectType,hash);
	    ObjectEntry oEntry = {name,(omrobjectptr_t)this,0};
		ObjectEntry *entryInTable = (ObjectEntry *)hashTableAdd(Heap::GetHeap()->getVM()->objectTable, &oEntry);
		/* update entry if it already exists in table */
		entryInTable->objPtr = (omrobjectptr_t)this;
}
void VMObject::SetNumberOfFields(uintptr_t nof) {
    this->numberOfFields = nof;

    for (int i = 0; i < nof ; ++i) {
        this->SetField(i, nilObject);
    }
}




void VMObject::Send(StdString selectorString, pVMObject* arguments, int argc) {
    pVMSymbol selector = _UNIVERSE->SymbolFor(selectorString);
    pVMFrame frame = _UNIVERSE->GetInterpreter()->GetFrame();
    frame->Push(this);

    for(int i = 0; i < argc; ++i) {
        frame->Push(arguments[i]);
    }

    pVMClass cl = this->GetClass();
    pVMInvokable invokable = (pVMInvokable)(cl->LookupInvokable(selector));
    (*invokable)(frame);
}


int VMObject::GetDefaultNumberOfFields() const {
	return VMObjectNumberOfFields; 
}

pVMClass VMObject::GetClass() const {
	return clazz;
}

void VMObject::SetClass(pVMClass cl) {
	clazz = cl;
}

pVMSymbol VMObject::GetFieldName(int index) const {
    return this->clazz->GetInstanceFieldName(index);
}

int VMObject::GetFieldIndex(pVMSymbol fieldName) const {
    return this->clazz->LookupFieldIndex(fieldName);
}

uintptr_t VMObject::GetNumberOfFields() const {
    return this->numberOfFields;
}

uintptr_t VMObject::GetObjectSize() const {
    return objectSize;
}


int32_t VMObject::GetGCField() const {
    return gcfield;
}

	
void VMObject::SetGCField(int32_t value) { 
    gcfield = value; 
}

void VMObject::SetObjectSize(size_t size) {
    objectSize = size; 
}

void VMObject::Assert(bool value) const {
    _UNIVERSE->Assert(value);
}


pVMObject VMObject::GetField(int index) const {
    return FIELDS[index]; 
}


void VMObject::SetField(int index, pVMObject value) {
     FIELDS[index] = value;
}

//returns the Object's additional memory used (e.g. for Array fields)
int VMObject::GetAdditionalSpaceConsumption() const
{
//	char * nm = NULL;
//	char * cls = "ClassNull";
//	if(this != NULL){
//		if( this->GetClass() != NULL){
//			if( this->GetClass()->GetName() != NULL){
//				if( this->GetClass()->GetName()->GetChars()!= NULL){
//					nm  = this->GetClass()->GetName()->GetChars();
//				}
//			}
//		}else{
//			nm = cls;
//		}
//	}
//
//	  uintptr_t size = extensions->objectModel.adjustSizeInBytes(objectSize);
//	
//				"class=%s,realsize=%d,objectSize=%d,sizeof(VMObject)=%d,sizeof(pVMObject)=%d,this.numberoffields=%d,return=%d\n"
//			,nm==NULL? "NULL":nm
//				,size
//					,objectSize
//					,sizeof(VMObject)
//					,sizeof(pVMObject)
//					,this->GetNumberOfFields()
//					,objectSize - (sizeof(VMObject) +
//	                        sizeof(pVMObject) * (this->GetNumberOfFields() - 1)));
    //The VM*-Object's additional memory used needs to be calculated.
    //It's      the total object size   MINUS   sizeof(VMObject) for basic
    //VMObject  MINUS   the number of fields times sizeof(pVMObject)
	//zg.this method didn't count the alignment .
//    return (objectSize - (sizeof(VMObject) +
//                          sizeof(pVMObject) * (this->GetNumberOfFields() - 1)));
	int rt = (objectSize - (sizeof(VMObject) +
            sizeof(pVMObject) * (this->GetNumberOfFields() - 1)));
    return rt;
}

// pVMObject       VMObject::GetMarkableFieldObj(int idx) const {
//	 int fn = GetNumberOfFields();
//	 int in = GetNumberOfIndexableFields();
//	 if(idx <fn ){
//		 return (pVMObject) FIELDS[idx];
//	 }else if (idx<fn+in){
//		 return (pVMObject)*(GetStartOfAdditionalPoint()+(idx-fn));
//	 }else {
//		 return NULL;
//	 }
// }

pVMObject       VMObject::GetNextMarkableField()  {
	//This method is too general, looks like every system objects need their own ways of this function. This function should fit for VMArray, VMFrame, VMClass, VMBlock
	//Also fit for VMMethod, which have different define of GetNumberOfIndexableFields()..
	 int fn = GetNumberOfFields();
	 int in = GetNumberOfIndexableFields();
	 pVMObject po = NULL;

	 if(markablefieldindex <fn ){
		 po =  (pVMObject) FIELDS[markablefieldindex];
	 }else if (markablefieldindex<fn+in){
		po = (pVMObject)*(GetStartOfAdditionalPoint()+(markablefieldindex-fn));
	 }else {
		po = NULL;
	 }
	 //printf("zg.VMObject::GetNextMarkableField,cp0,this=%p,markablefieldindex=%d,po=%p\n",this,markablefieldindex,po);
	 markablefieldindex++;
	 return po;
}


void VMObject::MarkReferences() {
    if (this->gcfield) return;
	this->SetGCField(1);
    for( int i = 0; i < this->GetNumberOfFields(); ++i) {
        pVMObject o = (FIELDS[i]);
        o->MarkReferences();
    }

}
