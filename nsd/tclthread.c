/*
 * The contents of this file are subject to the AOLserver Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://aolserver.com/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is AOLserver Code and related documentation
 * distributed by AOL.
 * 
 * The Initial Developer of the Original Code is America Online,
 * Inc. Portions created by AOL are Copyright (C) 1999 America Online,
 * Inc. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */


/*
 * tclthread.c --
 *
 *	Tcl wrappers around all thread objects 
 */

static const char *RCSID = "@(#) $Header: /Users/dossy/Desktop/cvs/aolserver/nsd/tclthread.c,v 1.22 2003/05/30 19:31:39 mpagenva Exp $, compiled: " __DATE__ " " __TIME__;

#ifdef NS_NOCOMPAT
#undef NS_NOCOMPAT
#endif
#include "nsd.h"

typedef struct ThreadArg {
    int detached;
    char *server;
    char script[1];
} ThreadArg;

/*
 * Local functions defined in this file
 */

static int GetAddr(Tcl_Interp *interp, int type, char *id, void **addrPtr);
static void SetAddr(Tcl_Interp *interp, int type, void *addr);
static int GetArgs(Tcl_Interp *interp, int objc, Tcl_Obj **objv,
	CONST char *opts[], int type, int create, int *optPtr, void **addrPtr);
static void CreateTclThread(NsInterp *itPtr, char *script, int detached,
			    Ns_Thread *thrPtr);

/*
 * The following define the address Tcl_Obj type.
 */

static int  SetAddrFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void UpdateStringOfAddr(Tcl_Obj *objPtr);
static void SetAddrInternalRep(Tcl_Obj *objPtr, int type, void *addr);
static int GetAddrFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int type, void **addrPtr);

static Tcl_ObjType addrType = {
    "ns:addr",
    (Tcl_FreeInternalRepProc *) NULL,
    (Tcl_DupInternalRepProc *) NULL,
    UpdateStringOfAddr,
    SetAddrFromAny
};


/*
 *----------------------------------------------------------------------
 *
 * NsTclInitAddrType --
 *
 *	Initialize the Tcl address object type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
NsTclInitAddrType(void)
{
    Tcl_RegisterObjType(&addrType);
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclMutexObjCmd --
 *
 *	Implements ns_mutex as obj command. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclMutexObjCmd(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj **objv)
{
    Ns_Mutex *lockPtr;
    static CONST char *opts[] = {
	"create", "destroy", "lock", "unlock", NULL
    };
    enum {
	MCreateIdx, MDestroyIdx, MLockIdx, MUnlockIdx
    } opt;

    if (!GetArgs(interp, objc, objv, opts, 'm', MCreateIdx,
		  (int *) &opt, (void **) &lockPtr)) {
	return TCL_ERROR;
    }
    switch (opt) {
    case MCreateIdx:
	Ns_MutexInit(lockPtr);
	if (objc > 2) {
	    Ns_MutexSetName(lockPtr, Tcl_GetString(objv[2]));
	}
	break;
    case MLockIdx:
	Ns_MutexLock(lockPtr);
	break;
    case MUnlockIdx:
	Ns_MutexUnlock(lockPtr);
	break;
    case MDestroyIdx:
	Ns_MutexDestroy(lockPtr);
        ns_free(lockPtr);
	break;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclCritSecObjCmd --
 *
 *	Implements ns_critsec. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See doc. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclCritSecObjCmd(ClientData data, Tcl_Interp *interp, int objc,
		   Tcl_Obj **objv)
{
    Ns_Cs *csPtr;
    static CONST char *opts[] = {
	"create", "destroy", "enter", "leave", NULL
    };
    enum {
	CCreateIdx, CDestroyIdx, CEnterIdx, CLeaveIdx
    } opt;

    if (!GetArgs(interp, objc, objv, opts, 'c', CCreateIdx,
		  (int *) &opt, (void **) &csPtr)) {
	return TCL_ERROR;
    }
    switch (opt) {
    case CCreateIdx:
	Ns_CsInit(csPtr);
	break;
    case CEnterIdx:
	Ns_CsEnter(csPtr);
	break;
    case CLeaveIdx:
	Ns_CsLeave(csPtr);
	break;
    case CDestroyIdx:
	Ns_CsDestroy(csPtr);
        ns_free(csPtr);
	break;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclSemaObjCmd --
 *
 *	Implements ns_sema. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclSemaObjCmd(ClientData data, Tcl_Interp *interp, int objc,
		Tcl_Obj **objv)
{
    Ns_Sema *semaPtr;
    int      cnt;
    static CONST char *opts[] = {
	"create", "destroy", "release", "wait", NULL
    };
    enum {
	SCreateIdx, SDestroyIdx, SReleaseIdx, SWaitIdx
    } opt;

    if (!GetArgs(interp, objc, objv, opts, 's', SCreateIdx,
		  (int *) &opt, (void **) &semaPtr)) {
	return TCL_ERROR;
    }
    switch (opt) {
    case SCreateIdx:
        if (objc < 3) {
            cnt = 0;
        } else if (Tcl_GetIntFromObj(interp, objv[2], &cnt) != TCL_OK) {
            return TCL_ERROR;
        }
	Ns_SemaInit(semaPtr, cnt);
	break;
    case SReleaseIdx:
        if (objc < 4) {
            cnt = 1;
        } else if (Tcl_GetIntFromObj(interp, objv[3], &cnt) != TCL_OK) {
            return TCL_ERROR;
        }
	Ns_SemaPost(semaPtr, cnt);
	break;
    case SWaitIdx:
	Ns_SemaWait(semaPtr);
	break;
    case SDestroyIdx:
	Ns_SemaDestroy(semaPtr);
        ns_free(semaPtr);
	break;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclCondObjCmd --
 *
 *	Implements ns_cond and ns_event.
 *
 * Results:
 *	See docs. 
 *
 * Side effects:
 *	See docs.
 *
 *----------------------------------------------------------------------
 */

int
NsTclCondObjCmd(ClientData data, Tcl_Interp *interp, int objc,
		Tcl_Obj **objv)
{
    Tcl_Obj *objPtr;
    Ns_Cond *condPtr;
    Ns_Mutex *lock;
    Ns_Time   timeout;
    int       result;
    static CONST char *opts[] = {
	"abswait", "broadcast", "create", "destroy", "set",
	"signal", "timedwait", "wait", NULL
    };
    enum {
	EAbsWaitIdx, EBroadcastIdx, ECreateIdx, EDestroyIdx, ESetIdx,
	ESignalIdx, ETimedWaitIdx, EWaitIdx
    } opt;

    if (!GetArgs(interp, objc, objv, opts, 'e', ECreateIdx,
		  (int *) &opt, (void **) &condPtr)) {
	return TCL_ERROR;
    }
    switch (opt) {
    case ECreateIdx:
	Ns_CondInit(condPtr);
	break;
    case EAbsWaitIdx:
    case ETimedWaitIdx:
    case EWaitIdx:
        if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "condId mutexId ?timeout?");
            return TCL_ERROR;
        }
	if (GetAddr(interp, 'm', Tcl_GetString(objv[3]), (void **) &lock) != TCL_OK) {
            return TCL_ERROR;
        }
        if (objc < 5) {
            timeout.sec = timeout.usec = 0;
        } else if (Ns_TclGetTimeFromObj(interp, objv[4], &timeout) != TCL_OK) {
            return TCL_ERROR;
        }
	if (opt == EAbsWaitIdx) {
            result = Ns_CondTimedWait(condPtr, lock, &timeout);
	} else if (opt == ETimedWaitIdx) {
	    Ns_Event *eventPtr = (Ns_Event *) condPtr;
            result = Ns_TimedWaitForEvent(eventPtr, lock, timeout.sec);
	} else {
	    if (objc < 5 || (timeout.sec == 0 && timeout.usec == 0)) {
		Ns_CondWait(condPtr, lock);
		result = NS_OK;
	    } else {
		Ns_Time abstime;
		Ns_GetTime(&abstime);
		Ns_IncrTime(&abstime, timeout.sec, timeout.usec);
		result = Ns_CondTimedWait(condPtr, lock, &abstime);
	    }
	}
	if (result == NS_OK) {
	    objPtr = Tcl_NewBooleanObj(1);
	} else if (result == NS_TIMEOUT) {
	    objPtr = Tcl_NewBooleanObj(0);
	} else {
            return TCL_ERROR;
        }
	Tcl_SetObjResult(interp, objPtr);
	break;

    case EBroadcastIdx:
	Ns_CondBroadcast(condPtr);
	break;

    case ESetIdx:
    case ESignalIdx:
	Ns_CondSignal(condPtr);
	break;

    case EDestroyIdx:
	Ns_CondDestroy(condPtr);
        ns_free(condPtr);
	break;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclRWLockObjCmd --
 *
 *	Implements ns_rwlock. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclRWLockObjCmd(ClientData data, Tcl_Interp *interp, int objc,
		  Tcl_Obj **objv)
{
    Ns_RWLock *rwlockPtr;
    static CONST char *opts[] = {
	"create", "destroy", "readlock", "readunlock",
	"writelock", "writeunlock", "unlock", NULL
    };
    enum {
	RCreateIdx, RDestroyIdx, RReadLockIdx, RReadUnlockIdx,
	RWriteLockIdx, RWriteUnlockIdx, RUnlockIdx
    } opt;

    if (!GetArgs(interp, objc, objv, opts, 'r', RCreateIdx,
		  (int *) &opt, (void **) &rwlockPtr)) {
	return TCL_ERROR;
    }
    switch (opt) {
    case RCreateIdx:
	Ns_RWLockInit(rwlockPtr);
	break;
    case RReadLockIdx:
	Ns_RWLockRdLock(rwlockPtr);
	break;
    case RWriteLockIdx:
	Ns_RWLockWrLock(rwlockPtr);
	break;
    case RReadUnlockIdx:
    case RWriteUnlockIdx:
    case RUnlockIdx:
	Ns_RWLockUnlock(rwlockPtr);
	break;
    case RDestroyIdx:
	Ns_RWLockDestroy(rwlockPtr);
	ns_free(rwlockPtr);
	break;
    }
    return TCL_OK;

}


/*
 *----------------------------------------------------------------------
 *
 * NsTclThreadCmd --
 *
 *	Implements ns_thread. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclThreadCmd(ClientData arg, Tcl_Interp *interp, int argc, char **argv)
{
    NsInterp *itPtr = arg;
    void *status;
    Ns_Thread tid;

    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
            argv[0], " command arg\"", NULL);
        return TCL_ERROR;
    }
    if (STREQ(argv[1], "begin") || STREQ(argv[1], "create") ||
	STREQ(argv[1], "begindetached")) {
        if (argc < 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                argv[0], " ", argv[1], " script\"", NULL);
            return TCL_ERROR;
        }
	if (STREQ(argv[1], "begindetached")) {
	    CreateTclThread(itPtr, argv[2], 1, NULL);
        } else {
	    CreateTclThread(itPtr, argv[2], 0, &tid);
            SetAddr(interp, 't', tid);
        }
    } else if (STREQ(argv[1], "wait") || STREQ(argv[1], "join")) {
        if (argc < 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                argv[0], " ", argv[1], " tid\"", NULL);
            return TCL_ERROR;
        }
        if (GetAddr(interp, 't', argv[2], (void **) &tid) 
	    != TCL_OK) {
            return TCL_ERROR;
        }
	Ns_ThreadJoin(&tid, &status);
	Tcl_SetResult(interp, (char *) status, (Tcl_FreeProc *) ns_free);
    } else if (STREQ(argv[1], "get")) {
        Ns_ThreadSelf(&tid);
        SetAddr(interp, 't', tid);
    } else if (STREQ(argv[1], "getid") || STREQ(argv[1], "id")) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(Ns_ThreadId()));
    } else if (STREQ(argv[1], "name")) {
	if (argc > 2) {
	    Ns_ThreadSetName(argv[2]);
	}
	Tcl_SetResult(interp, Ns_ThreadGetName(), TCL_VOLATILE);
    } else if (STREQ(argv[1], "yield")) {
        Ns_ThreadYield();
    } else {
        Tcl_AppendResult(interp, "unknown command \"",
            argv[1], "\":  should be begin, begindetached, create "
            "get, getid, id, join, wait, or yield", NULL);
        return TCL_ERROR;
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SetAddr --
 *
 *	Set the interp result with an opaque thread-object string id.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interp result set.
 *
 *----------------------------------------------------------------------
 */

static void
SetAddr(Tcl_Interp *interp, int type, void *addr)
{
    char buf[40];

    sprintf(buf, "%cid%p", type, addr);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
}


/*
 *----------------------------------------------------------------------
 *
 * GetObj --
 *
 *	Take an opaque thread-object Tcl handle and convert it into a 
 *	pointer. 
 *
 * Results:
 *	TCL_OK or TCL_ERROR 
 *
 * Side effects:
 *	An error will be put appended to the interp on failure 
 *
 *----------------------------------------------------------------------
 */

static int
GetAddr(Tcl_Interp *interp, int type, char *id, void **addrPtr)
{
    void *addr;

    if (*id++ != type || *id++ != 'i' || *id++ != 'd'
	|| sscanf(id, "%p", &addr) != 1 || addr == NULL) {
	Tcl_AppendResult(interp, "invalid thread object id \"",
	    id, "\"", NULL);
	return TCL_ERROR;
    }
    *addrPtr = addr;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_TclThread --
 *
 *	Run a Tcl script in a new thread. 
 *
 * Results:
 *	NS_OK. 
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

int
Ns_TclThread(Tcl_Interp *interp, char *script, Ns_Thread *thrPtr)
{
    NsInterp *itPtr = NsGetInterp(interp);

    CreateTclThread(itPtr, script, (thrPtr == NULL), thrPtr);
    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_TclDetachedThread --
 *
 *	Run a Tcl script in a detached thread. 
 *
 * Results:
 *	NS_OK. 
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

int
Ns_TclDetachedThread(Tcl_Interp *interp, char *script)
{
    return Ns_TclThread(interp, script, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * CreateTclThread --
 *
 *	Create a new Tcl thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on new thread script.
 *
 *----------------------------------------------------------------------
 */

static void
CreateTclThread(NsInterp *itPtr, char *script, int detached, Ns_Thread *thrPtr)
{
    ThreadArg *argPtr;

    argPtr = ns_malloc(sizeof(ThreadArg) + strlen(script));
    argPtr->detached = detached;
    strcpy(argPtr->script, script);
    if (itPtr != NULL && itPtr->servPtr != NULL) {
    	argPtr->server = itPtr->servPtr->server;
    } else {
    	argPtr->server = NULL;
    }
    Ns_ThreadCreate(NsTclThread, argPtr, 0, thrPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclThread --
 *
 *	Tcl thread main.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Copy of string result is return as exit arg to be reaped
 *	by ns_thread wait.
 *
 *----------------------------------------------------------------------
 */

void
NsTclThread(void *arg)
{
    ThreadArg *argPtr = arg;
    Ns_DString ds, *dsPtr;
    int        detached = argPtr->detached;

    if (detached) {
	dsPtr = NULL;
    } else {
	Ns_DStringInit(&ds);
	dsPtr = &ds;
    }

    /*
     * Need to ensure that the server has completed it's initializtion
     * prior to initiating TclEval.
     */
    Ns_WaitForStartup();

    (void) Ns_TclEval(dsPtr, argPtr->server, argPtr->script);
    ns_free(argPtr);
    if (!detached) {
	Ns_ThreadExit(Ns_DStringExport(&ds));
    }
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclThreadArgProc --
 *
 *	Proc info routine to copy Tcl thread script.
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	Will copy script to given dstring.
 *
 *----------------------------------------------------------------------
 */

void
NsTclThreadArgProc(Tcl_DString *dsPtr, void *arg)
{
    ThreadArg *argPtr = arg;

     Tcl_DStringAppendElement(dsPtr, argPtr->script);
}


static int
GetArgs(Tcl_Interp *interp, int objc, Tcl_Obj **objv, CONST char *opts[],
	 int type, int create, int *optPtr, void **addrPtr)
{
    Tcl_Obj *objPtr;
    int   opt;
    void *addr;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
        return 0;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], opts, "option", 0,
			    &opt) != TCL_OK) {
	return 0;
    }
    if (opt == create) {
	addr = ns_malloc(sizeof(void *));
	objPtr = Tcl_GetObjResult(interp);
	SetAddrInternalRep(objPtr, type, addr);
    } else {
        if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "object");
            return 0;
	}
    	if (GetAddrFromObj(interp, objv[2], type, &addr) != TCL_OK) {
	    return 0;
	}
    }
    *addrPtr = addr;
    *optPtr = opt;
    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * GetAddrFromObj --
 *
 *	Return the internal pointer of an address Tcl_Obj.
 *
 * Results:
 *	TCL_OK or TCL_ERROR if not a valid Ns_Time.
 *
 * Side effects:
 *	Object is set to id type if necessary.
 *
 *----------------------------------------------------------------------
 */

static int
GetAddrFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int type, void **addrPtr)
{
    if (Tcl_ConvertToType(interp, objPtr, &addrType) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((int) objPtr->internalRep.twoPtrValue.ptr1 != type) {
	Tcl_AppendResult(interp, "incorrect type: ", Tcl_GetString(objPtr), NULL);
	return TCL_ERROR;
    }
    *addrPtr = objPtr->internalRep.twoPtrValue.ptr2;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfAddr --
 *
 *	Update the string representation for an address object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the Ns_Time-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfAddr(objPtr)
    register Tcl_Obj *objPtr;	/* Int object whose string rep to update. */
{
    int type = (int) objPtr->internalRep.twoPtrValue.ptr1;
    void *addr = objPtr->internalRep.twoPtrValue.ptr2;
    char buf[40];
    size_t len;

    len = sprintf(buf, "%cid%p", type, addr);
    objPtr->bytes = ckalloc(len + 1);
    strcpy(objPtr->bytes, buf);
    objPtr->length = len;
}


/*
 *----------------------------------------------------------------------
 *
 * SetAddrFromAny --
 *
 *	Attempt to generate an address internal form for the Tcl object.
 *
 * Results:
 *	The return value is a standard object Tcl result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an int is stored as "objPtr"s internal
 *	representation. 
 *
 *----------------------------------------------------------------------
 */

static int
SetAddrFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    void *addr;
    int type;
    register char *id, *p;

    p = id = Tcl_GetString(objPtr);
    type = *p++;
    if (type == '\0' || *p++ != 'i' || *p++ != 'd'
	|| sscanf(p, "%p", &addr) != 1 || addr == NULL) {
	Tcl_AppendResult(interp, "invalid thread object id \"",
	    id, "\"", NULL);
	return TCL_ERROR;
    }
    SetAddrInternalRep(objPtr, type, addr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SetAddrInternalRep --
 *
 *	Set the internal address, freeing a previous internal rep if
 *	necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Object will be an addr type.
 *
 *----------------------------------------------------------------------
 */

static void
SetAddrInternalRep(Tcl_Obj *objPtr, int type, void *addr)
{
    Tcl_ObjType *typePtr = objPtr->typePtr;

    if (typePtr != NULL && typePtr->freeIntRepProc != NULL) {
	(*typePtr->freeIntRepProc)(objPtr);
    }
    objPtr->typePtr = &addrType;
    objPtr->internalRep.twoPtrValue.ptr1 = (void *) type;
    objPtr->internalRep.twoPtrValue.ptr2 = addr;
    Tcl_InvalidateStringRep(objPtr);
}
