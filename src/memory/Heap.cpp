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


#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "Heap.h"

#include "VMObject.h"
#include "VMSymbol.h"


#include "VMFreeObject.h"

#include "../vm/Universe.h"


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
#include "omrgc.h"
#include "omrvm.h"
#include "StartupManagerImpl.hpp"
#include "omrExampleVM.hpp"
#include "Heap.hpp"

/*
 * macro for padding - only word-aligned memory must be allocated
 */
#define PAD_BYTES(N) ((sizeof(void*) - ((N) % sizeof(void*))) % sizeof(void*))

Heap * Heap::theHeap = NULL;

Heap* Heap::GetHeap() {
    if (!theHeap) {
        _UNIVERSE->ErrorExit("Trying to access uninitialized Heap");
    }
    return theHeap;
}

void Heap::InitializeHeap( int objectSpaceSize,OMR_VM_Example * vm ) {
    if (theHeap) {
        cout << "Warning, reinitializing already initialized Heap, " 
             << "all data will be lost!" << endl;
       delete theHeap;
        //TODO: delete it from OMR heap?
    }

    theHeap = new Heap(objectSpaceSize,vm);
}

void Heap::DestroyHeap() {
    //if (theHeap) delete theHeap;
	//zg.Do nothing here , as we would shutdown the OMR in Universe::Quit() routine.
}

//zg.20160903.0012.Trying to merge code with OMR. Using the extensions->heap to replace the theHeap.

Heap::Heap(uintptr_t objectSpaceSize,OMR_VM_Example *vm) {
	_vm = vm;

	omr_error_t rc = OMR_ERROR_NONE;
	/* Initialize heap and collector */
	{//TODO: Why can not new it ? with a static operator new overriding.? Using the default 1M-2M
		/* This has to be done in local scope because MM_StartupManager has a destructor that references the OMR VM */
	}
	OMRPORT_ACCESS_FROM_OMRVM(_vm->_omrVM);
	omrtty_printf("OMR VM/GC INITIALIZED\n");
	uninterruptableCounter = 0;
	_vmthread = omrVMThread;
	/* Do stuff */

	env = MM_EnvironmentBase::getEnvironment(_vmthread);
	allocationInterface = env->_objectAllocationInterface;
	extensions = env->getExtensions();
	omrHeap = extensions ->getHeap();
//	omrtty_printf("configuration is %s\n", extensions->configuration->getBaseVirtualTypeId());
//	omrtty_printf("collector interface is %s\n", env->getExtensions()->collectorLanguageInterface->getBaseVirtualTypeId());
//	omrtty_printf("garbage collector is %s\n", env->getExtensions()->getGlobalCollector()->getBaseVirtualTypeId());
//	omrtty_printf("allocation interface is %s\n", allocationInterface->getBaseVirtualTypeId());
	 numAlloc = 0;
}

Heap::~Heap() {
	/*zg. Do nothing now.
    if (gcVerbosity > 0) {

        cout << "-- Heap statistics --" << endl;
        cout << "Total number of allocations: " << numAllocTotal << endl;
        cout << "Number of allocations since last collection: " 
             << numAlloc << endl;
        std::streamsize p = cout.precision();
        cout.precision(3);
        cout << "Used memory: " << spcAlloc << "/" 
             << this->objectSpaceSize << " (" 
             << ((double)spcAlloc/(double)this->objectSpaceSize)*100 << "%)" << endl;
        cout.precision(p);
        gc->PrintGCStat();
    }
	free(objectSpace);
    */
}

VMObject* Heap::AllocateObject(size_t s) {
//	
	uintptr_t size = extensions->objectModel.adjustSizeInBytes(s);
	//
	if( size < s){//Impossible,.
		printf("zg.ERROR!!%s,%s,%s\n",__FILE__,__FUNCTION__,__LINE__);
		exit(1);
	}
    VMObject* vmo = (VMObject*) Allocate(size);
    if(vmo != NULL){
    vmo->SetObjectSize(s);  //zg. save the requested size.
    //TODO: How to do so? zg.add this object into the objectTable.??How to define the name of it.

    }
    return vmo;
}

void* Heap::Allocate(size_t size) {
	if (size == 0) return NULL;
    if (size < sizeof(VMObject))  {
        //this will never happen, as all allocation is done for VMObjects
        return internalAllocate(size);
    }

	uintptr_t allocatedFlags = 0;
	MM_AllocateDescription mm_allocdescription(size, allocatedFlags, true, true);
	//omrobjectptr_t obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), false);
	//omrobjectptr_t obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), true);
	//omrobjectptr_t obj = (omrobjectptr_t)OMR_GC_AllocateNoGC(omrVMThread, OMR_EXAMPLE_ALLOCATION_CATEGORY, size, allocatedFlags);
	omrobjectptr_t obj = (omrobjectptr_t)OMR_GC_Allocate(omrVMThread, OMR_EXAMPLE_ALLOCATION_CATEGORY, size, allocatedFlags);
	if (NULL != obj) {
//		printf("zg.Heap::Allocate.cp0,obj=%p\n",obj);
//		MM_ObjectAllocationInterface *allocationInterface = env->_objectAllocationInterface;
//			MM_AllocationStats *allocationStats = allocationInterface->getAllocationStats();
//			printf("thread allocated %d tlh bytes, %d non-tlh bytes, from %d allocations before NULL\n",
//				allocationStats->tlhBytesAllocated(), allocationStats->nontlhBytesAllocated());
		extensions->objectModel.setObjectSize(obj, mm_allocdescription.getBytesRequested());
        addUninterruptableAllocationObject(obj);      // TODD @A1A Begin
		//((VMObject *) obj )->SetObjectSize(size);   //zg. no need to set.  as it's already in the first word( 4 bytes) .  The first byte is reserved for age&flag, and the remains 3 bytes are for size.
        globalObjectsAllocated++;
	}else{
		std::cout <<"ERROR: allocation failure."<<std::endl;
	}



#ifdef HEAPDEBUG 
    std::cout << "allocating: " << (int)size << "bytes" << std::endl;
#endif
    return (void *) obj;
    
}
/*
void Heap::PrintFreeList() {
    VMFreeObject* curEntry = freeListStart;
    int i =0;
    while (curEntry != NULL) {
        cout << "i: " << curEntry->GetObjectSize() << endl;
        ++i;
        curEntry = curEntry->GetNext();
    }
}
*/
void Heap::FullGC() {
	//TODO: How to do  manual GC on OMR?
   //OMR_GC_SystemCollect();
	extensions->heap->systemGarbageCollect(env,J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC);
	//From Liu Yan: TODO
//	MM_MemorySpace *memorySpace = env->getMemorySpace();
//	    memorySpace->localGarbageCollect(env);
   // gc->Collect();
}

void Heap::Free(void* ptr) {
	//TODO: how to check whether one ptr is in OMR heap or not?
	/*
    if ( ((int)ptr < (int) this->objectSpace) &&
        ((int)ptr > (int) this->objectSpace + this->objectSpaceSize)) {
        internalFree(ptr);
    }
    */
}

void Heap::Destroy(VMObject* _object) {
	//todo: How to destroy one object from OMR heap?
	/*
    int freedBytes = _object->GetObjectSize();
    memset(_object, 0, freedBytes);
    VMFreeObject* object = (VMFreeObject*) _object;

    //see if there's an adjoining unused object behind this object
    VMFreeObject* next = (VMFreeObject*)((int)object + (int)freedBytes);
    if (next->GetGCField() == -1) {
        //yes, there is, so we can join them
        object->SetObjectSize(next->GetObjectSize() + freedBytes);
        object->SetNext(next->GetNext());
        next->GetNext()->SetPrevious(object);
        VMFreeObject* previous = next->GetPrevious();
        object->SetPrevious(previous);
        memset(next, 0, next->GetObjectSize());
    } else {
        //no, there is not, so we just put the new unused object as the new freeListStart
        object->SetObjectSize(freedBytes);
        object->SetNext(freeListStart);
        freeListStart->SetPrevious(object);
        freeListStart = object;
    }*/

    //TODO: find a way to merge unused objects that are before this object
}

void Heap::internalFree(void* ptr) {
    free(ptr);
}

void* Heap::internalAllocate(size_t size) {
    if (size == 0) return NULL;
    void* result = malloc(size);
    if(!result) {
        cout << "Failed to allocate " << size << " Bytes." << endl;
        _UNIVERSE->Quit(-1);
    }
    memset(result, 0, size);
    return result;
}

// TODD @A1A Begin
void Heap::StartUninterruptableAllocation() 
{ 
    ++uninterruptableCounter; 
}


void Heap::EndUninterruptableAllocation() 
{   
    if( (--uninterruptableCounter) == 0 )
    {   removeAllUninterruptableAllocationObject();        
    }
}
// TODD @A1A End
