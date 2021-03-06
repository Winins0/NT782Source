/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 Copyright (c) 1989 Microsoft Corporation

 Module Name:
	
	fldattr.cxx

 Abstract:

	field attribute handling routines

 Notes:


 Author:

	GregJen	Oct-27-1993	Created.

 Notes:


 ----------------------------------------------------------------------------*/

/****************************************************************************
 *	include files
 ***************************************************************************/

#include "allnodes.hxx"
#include "fldattr.hxx"
#include "semantic.hxx"

/****************************************************************************
 *	local data
 ***************************************************************************/

expr_constant	*	pZero	= NULL;
expr_constant	*	pOne	= NULL;

/****************************************************************************
 *	externs
 ***************************************************************************/

/****************************************************************************
 *	definitions
 ***************************************************************************/


/////////////////
// helper routines for Normalize functions; 

//Don't add or subtract 0.
expr_node	*
CreateSimpleBinaryArithExpression( 
	OPERATOR 		Op,
	expr_node	* 	pL,
	expr_node	* 	pR
	)
{
	assert( (Op==OP_PLUS) || (Op==OP_MINUS) );
	assert( pL );
	assert( pR );

	if ( pR->IsConstant() && (pR->GetValue() == 0) )
		return pL;

	if ( pL->IsConstant() && (pL->GetValue() == 0) )
		return pR;

	return new expr_b_arithmetic( Op, pL, pR );

}


// return the constant 0 over and over
expr_constant	*
GetConstant0()
{
	if ( pZero )	return pZero;

	pZero	= new expr_constant( 0L, VALUE_TYPE_NUMERIC );
	return pZero; 
}
	
// return the constant 1 over and over
expr_constant	*
GetConstant1()
{
	if ( pOne )	return pOne;

	pOne	= new expr_constant( 1L, VALUE_TYPE_NUMERIC );
	return pOne; 
}
	

BOOL
IsInValidOutOnly( SEM_ANALYSIS_CTXT * pCtxt )
{
	// an out-only size is valid only on out-only non-top-level things

	// in, in/out things not allowed
	if ( pCtxt->AnyAncestorBits( UNDER_IN_PARAM ) )
		return TRUE;

	// look up the stack for a pointer (or array) that is unique
	SEM_ANALYSIS_CTXT	*	pCurCtxt = pCtxt;
	NODE_T					Kind;
	node_skl			*	pNode;
	while ( pCurCtxt )
		{
		pNode	= pCurCtxt->GetParent();
		Kind	= pNode->NodeKind();

		switch ( Kind )
			{
			case NODE_DEF:
			case NODE_ARRAY:
				break;
			case NODE_POINTER:
				if ( pCtxt->AnyAncestorBits( IN_NON_REF_PTR ) )
					return FALSE;
				break;
			case NODE_PARAM:
			case NODE_PROC:
			default:
				return TRUE;
			}
		pCurCtxt = ( SEM_ANALYSIS_CTXT	* ) pCurCtxt->GetParentContext();
		}
	return TRUE;
}

BOOL Xxx_Is_Type_OK( node_skl * pType )
{
	if ( !pType )
		return FALSE;

	while ( TRUE )
		{
		switch ( pType->NodeKind() )
			{
			case NODE_PARAM:
			case NODE_FIELD:
				if ( !pType->GetChild() )
					return FALSE;

				break;

			// make sure that there is no transmit_as or represent_as
			case NODE_DEF:
				if ( pType->FInSummary( ATTR_TRANSMIT ) ||
				     pType->FInSummary( ATTR_REPRESENT_AS ) )
					return FALSE;

				break;
			
			// for an ID, make sure it is a const decl, then use its type
			case NODE_ID:
				{
				node_id		*	pID	= (node_id *) pType;
				if ( !pID->pInit )
					return FALSE;

				break;
				}
			case NODE_ENUM:
			case NODE_HYPER:
			case NODE_INT64:
			case NODE_LONG:
			case NODE_LONGLONG:
			case NODE_SHORT:
			case NODE_INT:
			case NODE_SMALL:
			case NODE_CHAR:
			case NODE_BOOLEAN:
			case NODE_BYTE:
				return TRUE;

			default:
				return FALSE;
			}
		pType = pType->GetChild();
		}

	return TRUE;
}


BOOL IID_Is_Type_OK( node_skl * pType )
{
	if ( !pType )
		return FALSE;

	while ( TRUE )
		{
		switch ( pType->NodeKind() )
			{
			case NODE_PARAM:
			case NODE_FIELD:
				if ( !pType->GetChild() )
					return FALSE;

				break;

			case NODE_DEF:
				if ( pType->FInSummary( ATTR_TRANSMIT ) ||
				     pType->FInSummary( ATTR_REPRESENT_AS ) )
					return FALSE;

				break;
			
			case NODE_POINTER:
				if ( pType->GetChild() )
					return ( 16 == pType->GetChild()->GetSize(0) );
				
			default:
				return FALSE;
			}
		pType = pType->GetChild();
		}

	return TRUE;
}


// validate the bunch of attributes for pointers: check combinations, ranges, 
// and expressions

void			
FIELD_ATTR_INFO::Validate( SEM_ANALYSIS_CTXT * pCtxt )
{
	if ( Kind == FA_NONE )
		return;

	node_skl	*		pParent		= pCtxt->GetParent();

// things to check:
// expression types (must be integral types)
	
	if ( pMaxIsExpr )
		{
		EXPR_CTXT		MaxCtxt( pCtxt );

		pMaxIsExpr->ExprAnalyze( &MaxCtxt );

		if ( MaxCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( MaxCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) && IsInValidOutOnly( pCtxt ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !MaxCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pMaxIsExpr->GetType() ) &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pMinIsExpr )
		{
		EXPR_CTXT		MinCtxt( pCtxt );

		pMinIsExpr->ExprAnalyze( &MinCtxt );

		if ( MinCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( MinCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) && IsInValidOutOnly( pCtxt ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !MinCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pMinIsExpr->GetType() ) &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pSizeIsExpr )
		{
		EXPR_CTXT		SizeCtxt( pCtxt );

		pSizeIsExpr->ExprAnalyze( &SizeCtxt );

		if ( SizeCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( SizeCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) && IsInValidOutOnly( pCtxt ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !SizeCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pSizeIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pFirstIsExpr )
		{
		EXPR_CTXT		FirstCtxt( pCtxt );

		pFirstIsExpr->ExprAnalyze( &FirstCtxt );

		if ( FirstCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !FirstCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pFirstIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pLastIsExpr )
		{
		EXPR_CTXT		LastCtxt( pCtxt );

		pLastIsExpr->ExprAnalyze( &LastCtxt );

		if ( LastCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !LastCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pLastIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pLengthIsExpr )
		{
		EXPR_CTXT		LengthCtxt( pCtxt );

		pLengthIsExpr->ExprAnalyze( &LengthCtxt );

		if ( LengthCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !LengthCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pLengthIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pIIDIsExpr )
		{
		EXPR_CTXT		IIDCtxt( pCtxt );

		pIIDIsExpr->ExprAnalyze( &IIDCtxt );

		if ( IIDCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !IIDCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !IID_Is_Type_OK( pIIDIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, IID_IS_EXPR_NON_POINTER, NULL );

		}

// min_is == 0 ( for now )
// constant min_is <= constant max_is
// size_is not with max_is
	if ( pMaxIsExpr && pSizeIsExpr )
		SemError( pParent, *pCtxt, MAX_AND_SIZE, NULL );

// min_is not alone

// constant first_is <= constant last_is + 1
// length_is not with last_is
	if ( pLengthIsExpr && pLastIsExpr )
		SemError( pParent, *pCtxt, LAST_AND_LENGTH, NULL );

// constant first_is, last_is both within min<->max range
// length_is <= size_is 
// string attrs not with varying attrs
// string and bstring not together

// conformant strings may leave out size_is if [in] or [in,out]

// accept the NULL value ( turn expression back null, clear kind bits )
// make sure variables come from the correct context

// lengthed, unsized pointer

	if ( ( pLengthIsExpr || pFirstIsExpr || pLastIsExpr ) &&
		!( pSizeIsExpr || pMaxIsExpr ) )
		SemError( pParent, *pCtxt, UNSIZED_ARRAY, NULL );



}

void			
FIELD_ATTR_INFO::Validate( SEM_ANALYSIS_CTXT * pCtxt, 
							  expr_node * pLower, 
							  expr_node * pUpper )
{
	node_skl	*		pParent		= pCtxt->GetParent();
	BOOL				OneBound = ( pLower == (expr_node *) 0 );

	if ( pUpper	== (expr_node *) -1 )
		{
		pUpper	=	NULL;
		Kind	|=	FA_CONFORMANT;
		}
	else if ( pUpper )
		{
		if ( pUpper->GetValue() <= 0 )
			{
			SemError( pParent, *pCtxt, ILLEGAL_ARRAY_BOUNDS, NULL );
			}
		}
		
	if (   pLower && 
		 ( pLower != (expr_node *) -1 ) &&
		 ( pLower->GetValue() != 0 ) )
		{
		SemError( pParent, *pCtxt, ARRAY_BOUNDS_CONSTRUCT_BAD, NULL );
		}

	if ( pUpper && ( pMaxIsExpr || pSizeIsExpr ) )
		{
		SemError( pParent, *pCtxt, SIZING_ON_FIXED_ARRAYS, NULL );
		}

// things to check:
// expression types (must be integral types)
	
	if ( pMaxIsExpr )
		{
		EXPR_CTXT		MaxCtxt( pCtxt );

		pMaxIsExpr->ExprAnalyze( &MaxCtxt );

		if ( MaxCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( MaxCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !MaxCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pMaxIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pMinIsExpr )
		{
		EXPR_CTXT		MinCtxt( pCtxt );

		pMinIsExpr->ExprAnalyze( &MinCtxt );

		if ( MinCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( MinCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !MinCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pMinIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pSizeIsExpr )
		{
		EXPR_CTXT		SizeCtxt( pCtxt );

		pSizeIsExpr->ExprAnalyze( &SizeCtxt );

		if ( SizeCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( SizeCtxt.AnyUpFlags( EX_OUT_ONLY_PARAM ) )
			SemError( pParent, *pCtxt, SIZE_SPECIFIER_CANT_BE_OUT, NULL );

		if ( !SizeCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pSizeIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}


	if ( pFirstIsExpr )
		{
		EXPR_CTXT		FirstCtxt( pCtxt );

		pFirstIsExpr->ExprAnalyze( &FirstCtxt );

		if ( FirstCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !FirstCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pFirstIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pLastIsExpr )
		{
		EXPR_CTXT		LastCtxt( pCtxt );

		pLastIsExpr->ExprAnalyze( &LastCtxt );

		if ( LastCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !LastCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pLastIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}

	if ( pLengthIsExpr )
		{
		EXPR_CTXT		LengthCtxt( pCtxt );

		pLengthIsExpr->ExprAnalyze( &LengthCtxt );

		if ( LengthCtxt.AnyUpFlags( EX_UNSAT_FWD ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_UNRESOLVED, NULL );

		if ( !LengthCtxt.AnyUpFlags( EX_VALUE_INVALID ) )
			SemError( pParent, *pCtxt, ATTRIBUTE_ID_MUST_BE_VAR, NULL );

		if ( !Xxx_Is_Type_OK( pLengthIsExpr->GetType() )  &&
			 pCtxt->AnyAncestorBits( IN_RPC ) )
			SemError( pParent, *pCtxt, ATTR_MUST_BE_INT, NULL );

		}


	if ( pIIDIsExpr )
		{
		SemError( pParent, *pCtxt, IID_IS_NON_POINTER, NULL );
		}

// min_is == 0 ( for now )
// constant min_is <= constant max_is
// size_is not with max_is
	if ( pMaxIsExpr && pSizeIsExpr )
		SemError( pParent, *pCtxt, MAX_AND_SIZE, NULL );

	// a conformant unsized array:
	//		must have string
	//		must not be out_only
	if ( ( Kind & FA_CONFORMANT ) && !pMaxIsExpr && !pSizeIsExpr && !pUpper )
		{
		if ( !( Kind & FA_STRING ) )
			{
			if ( pCtxt->AnyAncestorBits( IN_RPC ) )
				SemError( pParent, *pCtxt, UNSIZED_ARRAY, NULL );
			}
		else
			{
			if ( pCtxt->AllAncestorBits( IN_RPC |
										 IN_PARAM_LIST |
										 UNDER_OUT_PARAM ) &&
				 !pCtxt->AnyAncestorBits( UNDER_IN_PARAM ) )
				SemError( pParent, *pCtxt, DERIVES_FROM_UNSIZED_STRING, NULL );
			}
		}
		

// min_is not alone

// constant first_is <= constant last_is + 1
// length_is not with last_is
	if ( pLengthIsExpr && pLastIsExpr )
		SemError( pParent, *pCtxt, LAST_AND_LENGTH, NULL );

// constant first_is, last_is both within min<->max range
// length_is <= size_is 
// string attrs not with varying attrs
// string and bstring not together

// conformant strings may leave out size_is if [in] or [in,out]

// accept the NULL value ( turn expression back null, clear kind bits )
// make sure variables come from the correct context

}


//
// normalize for pointers ( no default bound )
void
FIELD_ATTR_INFO::Normalize()
{
	expr_node	*	pTmp1;
	expr_node	*	pTmp2;

	// convert the set: min_is, max_is, size_is to 			min_is + size_is

	if ( Kind & FA_CONFORMANT )
		{
		// default min_is is 0
		if ( ! pMinIsExpr )
			{
			pMinIsExpr		=	GetConstant0();
			}

		// size_is = (max_is - min_is) + 1;
		if ( ! pSizeIsExpr )
			{
			if ( pMaxIsExpr )
				{
				pTmp1		=	GetConstant1();
				pTmp2		=	CreateSimpleBinaryArithExpression( OP_MINUS, pMaxIsExpr, pMinIsExpr);
				pSizeIsExpr	=	CreateSimpleBinaryArithExpression( OP_PLUS, pTmp2, pTmp1 );
				}
			}
		}


	// convert the set: first_is, last_is, length_is to:	first_is + length_is
	if ( Kind & FA_VARYING )
		{
		// default first_is is 0
		if ( ! pFirstIsExpr )
			{
			pFirstIsExpr	=	GetConstant0();
			}

		// default last_is is max_is or size_is+1
		if ( ! pLastIsExpr )
			{
			if ( pMaxIsExpr )
				pLastIsExpr	= pMaxIsExpr;
			else if ( pSizeIsExpr )
				pLastIsExpr = CreateSimpleBinaryArithExpression( OP_MINUS, pSizeIsExpr, GetConstant1() ); 
			}

		// length_is = (last_is - first_is) + 1;
		if ( ! pLengthIsExpr )
			{
			if ( pLastIsExpr )
				{
				pTmp1			=	GetConstant1();
				pTmp2			=	CreateSimpleBinaryArithExpression( OP_MINUS, pLastIsExpr, pFirstIsExpr);
				pLengthIsExpr	=	CreateSimpleBinaryArithExpression( OP_PLUS, pTmp2, pTmp1 );
				}
			}
		}

}

// normalize for arrays (provided lower and upper bound)
void
FIELD_ATTR_INFO::Normalize(expr_node * pLower, expr_node * pUpper)
{
	expr_node	*	pTmp1;
	expr_node	*	pTmp2;
	BOOL			OneBound = FALSE;

	if ( pLower == (expr_node *) 0 )
		{
		pLower	=	GetConstant0();
		OneBound	= TRUE;
		}

	if ( pUpper	== (expr_node *) -1 )
		{
		pUpper	=	NULL;
		Kind	|=	FA_CONFORMANT;
		}

	// convert the set: min_is, max_is, size_is to:	min_is + size_is

	// first, copy from the bounds
	if ( ! pMinIsExpr )
		{
		pMinIsExpr		=	pLower;
		}

	if ( ! pMaxIsExpr && ! pSizeIsExpr && pUpper )
		{
		if (OneBound)
			{
			pTmp1		= GetConstant1();
			pMaxIsExpr	= CreateSimpleBinaryArithExpression( OP_MINUS, pUpper, pTmp1 );
			}
		else
			{
			pMaxIsExpr	= pUpper; 
			}
		}

	// size_is = (max_is - min_is) + 1;
	if ( ! pSizeIsExpr )
		{
		if ( pMaxIsExpr )
			{
			pTmp1		= GetConstant1();
			pTmp2		= CreateSimpleBinaryArithExpression( OP_MINUS, pMaxIsExpr, pMinIsExpr);
			pSizeIsExpr	= CreateSimpleBinaryArithExpression( OP_PLUS, pTmp2, pTmp1 );
			}
		}


	// convert the set: first_is, last_is, length_is to:	first_is + length_is

	// default first_is is min_is
	if ( ! pFirstIsExpr )
		{
		pFirstIsExpr	= pMinIsExpr;
		}

	// default last_is is max_is or size_is+1
	if ( ! pLastIsExpr )
		{
		if ( pMaxIsExpr )
			pLastIsExpr	= pMaxIsExpr;
		else if ( pSizeIsExpr )
			pLastIsExpr = CreateSimpleBinaryArithExpression( OP_MINUS, pSizeIsExpr, GetConstant1() ); 
		}

	// length_is = (last_is - first_is) + 1;
	if ( ! pLengthIsExpr )
		{
		if ( pLastIsExpr )
			{
			pTmp1			= GetConstant1();
			pTmp2			= CreateSimpleBinaryArithExpression( OP_MINUS, pLastIsExpr, pFirstIsExpr);
			pLengthIsExpr	= CreateSimpleBinaryArithExpression( OP_PLUS, pTmp2, pTmp1 );
			}
		}
		
}





