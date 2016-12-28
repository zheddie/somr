#pragma once
#ifndef VMINVOKABLE_H_
#define VMINVOKABLE_H_

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


#include "ObjectFormats.h"
#include "VMObject.h"
class VMSymbol;
class VMClass;
class VMFrame;

class VMInvokable : public VMObject {
public:
    //VMInvokable(int nof = 0) : VMObject(nof + 2){};
    VMInvokable(int nof = 0) : VMObject(nof + 2){
    	strcpy(objectType,"VMInvokable");
    };
    //virtual operator "()" to invoke the invokable
    virtual void      operator()(pVMFrame) = 0;

    virtual bool      IsPrimitive() const;
	virtual pVMSymbol GetSignature() const;
	virtual void      SetSignature(pVMSymbol sig);
	virtual pVMClass  GetHolder() const;
	virtual void      SetHolder(pVMClass hld);
    
protected:
	pVMSymbol signature;
	pVMClass holder;
};


#endif