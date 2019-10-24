/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#if !defined(OBJECTITERATOR_HPP_)
#define OBJECTITERATOR_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "modronbase.h"

#include "Base.hpp"
#include "GCExtensionsBase.hpp"
#include "objectdescription.h"
#include "ObjectModel.hpp"
#include "SlotObject.hpp"
#include "VMObject.h"

class GC_ObjectIterator
{
/* Data Members */
private:
protected:
	GC_SlotObject _slotObject;	/**< Create own SlotObject class to provide output */
	fomrobject_t *_scanPtr;			/**< current scan pointer */
	fomrobject_t *_endPtr;			/**< end scan pointer */
	pVMObject _obj;
public:

/* Member Functions */
private:
	MMINLINE void
	initialize(OMR_VM *omrVM, omrobjectptr_t objectPtr)
	{
		/* Start _scanPtr after header */
		_obj = (VMObject *)objectPtr;
		((VMObject * )objectPtr)->SetMarkableFieldIndex(0);  //We prefer to start from clazz, as some application class need to be kept during GC.
	}

protected:
public:

	/**
	 * Return back SlotObject for next reference
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject *nextSlot()
	{

		pVMObject po = _obj->GetNextMarkableField();
		if (po != NULL){
			_slotObject.writeAddressToSlot((fomrobject_t *)po);
			return &_slotObject;
		}else{
			return NULL;
		}
	}

	/**
	 * Restore iterator state: nextSlot() will be at this index
	 * @param index[in] The index of slot to be restored
	 */
	MMINLINE void restore(int32_t index)
	{
		//_scanPtr += index;
		_obj->SetMarkableFieldIndex(index);
	}

	/**
	 * @param omrVM[in] pointer to the OMR VM
	 * @param objectPtr[in] the object to be processed
	 */
	GC_ObjectIterator(OMR_VM *omrVM, omrobjectptr_t objectPtr)
		: _slotObject(GC_SlotObject(omrVM, NULL))
		, _scanPtr(NULL)
		, _endPtr(NULL)
	{
		initialize(omrVM, objectPtr);
	}
};

#endif /* OBJECTITERATOR_HPP_ */
