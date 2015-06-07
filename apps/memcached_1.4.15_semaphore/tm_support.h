/// These defines were copied from the gcc libitm.h file.  They provide the
/// minimal infrastructure needed for using onCommit handlers from code that
/// can be called from transactional and nontransactional contexts.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __i386__
/* Only for 32-bit x86.  */
# define ITM_REGPARM  __attribute__((regparm(2)))
#else
# define ITM_REGPARM
#endif

/* Results from inTransaction */
typedef enum
{
    outsideTransaction = 0,    /* So "if (inTransaction(td))" works */
    inRetryableTransaction,
    inIrrevocableTransaction
} _ITM_howExecuting;

typedef void (* _ITM_userCommitFunction) (void *);

typedef uint64_t _ITM_transactionId_t;  /* Transaction identifier */

extern _ITM_howExecuting _ITM_inTransaction(void) ITM_REGPARM;

extern void _ITM_addUserCommitAction(_ITM_userCommitFunction,
             _ITM_transactionId_t, void *) ITM_REGPARM;

#ifdef __cplusplus
} /* extern "C" */
#endif

