/**********************************************************************
** This program is part of 'MOOSE', the
** Messaging Object Oriented Simulation Environment.
**           Copyright (C) 2003-2009 Upinder S. Bhalla. and NCBS
** It is made available under the terms of the
** GNU Lesser General Public License version 2.1
** See the file COPYING.LIB for the full notice.
**********************************************************************/

#include "header.h"
#include "MsgManager.h"
#include "SparseMatrix.h"
#include "SparseMsg.h"
#include "../randnum/randnum.h"
#include "../biophysics/Synapse.h"

Id SparseMsg::id_;

//////////////////////////////////////////////////////////////////
//    MOOSE wrapper functions for field access.
//////////////////////////////////////////////////////////////////

const Cinfo* SparseMsg::initCinfo()
{
	///////////////////////////////////////////////////////////////////
	// Field definitions.
	///////////////////////////////////////////////////////////////////
	static ReadOnlyValueFinfo< SparseMsg, unsigned int > numRows(
		"numRows",
		"Number of rows in matrix.",
		&SparseMsg::getNumRows
	);
	static ReadOnlyValueFinfo< SparseMsg, unsigned int > numColumns(
		"numColumns",
		"Number of columns in matrix.",
		&SparseMsg::getNumColumns
	);
	static ReadOnlyValueFinfo< SparseMsg, unsigned int > numEntries(
		"numEntries",
		"Number of Entries in matrix.",
		&SparseMsg::getNumEntries
	);

	static ValueFinfo< SparseMsg, double > probability(
		"probability",
		"connection probability for random connectivity.",
		&SparseMsg::setProbability,
		&SparseMsg::getProbability
	);

	static ValueFinfo< SparseMsg, long > seed(
		"seed",
		"Random number seed for generating probabilistic connectivity.",
		&SparseMsg::setSeed,
		&SparseMsg::getSeed
	);

////////////////////////////////////////////////////////////////////////
// DestFinfos
////////////////////////////////////////////////////////////////////////

	static DestFinfo setRandomConnectivity( "setRandomConnectivity",
		"Assigns connectivity with specified probability and seed",
		new OpFunc2< SparseMsg, double, long >( 
		&SparseMsg::setRandomConnectivity ) );

	static DestFinfo setEntry( "setEntry",
		"Assigns single row,column value",
		new OpFunc3< SparseMsg, unsigned int, unsigned int, unsigned int >( 
		&SparseMsg::setEntry ) );

	static DestFinfo unsetEntry( "unsetEntry",
		"Clears single row,column entry",
		new OpFunc2< SparseMsg, unsigned int, unsigned int >( 
		&SparseMsg::unsetEntry ) );

	static DestFinfo clear( "clear",
		"Clears out the entire matrix",
		new OpFunc0< SparseMsg >( 
		&SparseMsg::clear ) );

	static DestFinfo transpose( "transpose",
		"Transposes the sparse matrix",
		new OpFunc0< SparseMsg >( 
		&SparseMsg::transpose ) );

////////////////////////////////////////////////////////////////////////
// Assemble it all.
////////////////////////////////////////////////////////////////////////

	static Finfo* sparseMsgFinfos[] = {
		&numRows,			// readonly value
		&numColumns,		// readonly value
		&numEntries,		// readonly value
		&probability,		// value
		&seed,				// value
		&setRandomConnectivity,	// dest
		&setEntry,			// dest
		&unsetEntry,		//dest
		&clear,				//dest
		&transpose,			//dest
	};

	static Cinfo sparseMsgCinfo (
		"SparseMsg",					// name
		MsgManager::initCinfo(),		// base class
		sparseMsgFinfos,
		sizeof( sparseMsgFinfos ) / sizeof( Finfo* ),	// num Fields
		new Dinfo< SparseMsg >()
	);

	return &sparseMsgCinfo;
}

static const Cinfo* sparseMsgCinfo = SparseMsg::initCinfo();

//////////////////////////////////////////////////////////////////
//    Value Fields
//////////////////////////////////////////////////////////////////
void SparseMsg::setProbability ( double probability )
{
	p_ = probability;
	mtseed( seed_ );
	randomConnect( probability );
}

double SparseMsg::getProbability ( ) const
{
	return p_;
}

void SparseMsg::setSeed ( long seed )
{
	seed_ = seed;
	mtseed( seed_ );
	randomConnect( p_ );
}

long SparseMsg::getSeed () const
{
	return seed_;
}

unsigned int SparseMsg::getNumRows() const
{
	return getMatrix().nRows();
}

unsigned int SparseMsg::getNumColumns() const
{
	return getMatrix().nColumns();
}

unsigned int SparseMsg::getNumEntries() const
{
	return getMatrix().nEntries();
}

//////////////////////////////////////////////////////////////////
//    DestFields
//////////////////////////////////////////////////////////////////

void SparseMsg::setRandomConnectivity( double probability, long seed )
{
	p_ = probability;
	seed_ = seed;
	mtseed( seed );
	randomConnect( probability );
}

void SparseMsg::setEntry(
	unsigned int row, unsigned int column, unsigned int value )
{
	matrix_.set( row, column, value );
}

void SparseMsg::unsetEntry( unsigned int row, unsigned int column )
{
	matrix_.unset( row, column );
}

void SparseMsg::clear()
{
	matrix_.clear();
}

void SparseMsg::transpose()
{
	matrix_.transpose();
}

//////////////////////////////////////////////////////////////////
//    Here are the actual class functions
//////////////////////////////////////////////////////////////////


SparseMsg::SparseMsg( MsgId mid, Element* e1, Element* e2 )
	: Msg( mid, e1, e2, id_ ),
	matrix_( e1->dataHandler()->parentDataHandler()->totalEntries(), 
		e2->dataHandler()->parentDataHandler()->totalEntries() )
{
	assert( e1->dataHandler()->parentDataHandler()->numDimensions() >= 1  && 
		e2->dataHandler()->parentDataHandler()->numDimensions() >= 1 );
}

SparseMsg::~SparseMsg()
{
	MsgManager::dropMsg( mid() );
}

unsigned int rowIndex( const Element* e, const DataId& d )
{
	if ( e->dataHandler()->numDimensions() == 1 ) {
		return d.data();
	} else if ( e->dataHandler()->numDimensions() == 2 ) {
		// rectangular grid, looking for index
		unsigned int row = e->dataHandler()->sizeOfDim(0) * d.data();
		/*
		unsigned int row = 0;
		for ( unsigned int i = 0; i < d.data(); ++i )
			row += e->dataHandler()->numData2( i );
		*/
		return ( row + d.field() );
	}
	return 0;
}

void SparseMsg::exec( const char* arg, const ProcInfo *p ) const
{
	const Qinfo *q = ( reinterpret_cast < const Qinfo * >( arg ) );
	// arg += sizeof( Qinfo );
	bool report = 1;

	/**
	 * The system is really optimized for data from e1 to e2.
	 */
	if ( q->isForward() ) {
		const OpFunc* f = e2_->cinfo()->getOpFunc( q->fid() );
		unsigned int row = rowIndex( e1_, q->srcIndex() );
		// unsigned int oldRow = row;

		const unsigned int* fieldIndex;
		const unsigned int* colIndex;
		unsigned int n = matrix_.getRow( row, &fieldIndex, &colIndex );

		if ( e1_->getName() == "test2" )
			report = 0;

		if ( report ) cout << p->nodeIndexInGroup << "." << p->threadIndexInGroup << ": SparseMsg " << e1_->getName() << "->" << e2_->getName() << ", row= " << row << ": entries= ";
		// J counts over all the column entries, i.e., all targets.
		for ( unsigned int j = 0; j < n; ++j ) {
			if ( report ) cout << "(" << colIndex[j] << "," << fieldIndex[j] << ")";
			if ( p->execThread( e2_->id(), colIndex[j] ) ) {
				Eref tgt( e2_, DataId( colIndex[j], fieldIndex[j] ) );
				if ( tgt.isDataHere() )
					f->op( tgt, arg );
			}
		}
		if ( report ) cout << "\n";
	} else {
		// Avoid using this back operation!
		// Note that we do NOT use the fieldIndex going backward. It is
		// assumed that e1 does not have fieldArrays.
		const OpFunc* f = e1_->cinfo()->getOpFunc( q->fid() );
		unsigned int column = rowIndex( e2_, q->srcIndex() );
		vector< unsigned int > fieldIndex;
		vector< unsigned int > rowIndex;
		unsigned int n = matrix_.getColumn( column, fieldIndex, rowIndex );
		for ( unsigned int j = 0; j < n; ++j ) {
			if ( p->execThread( e1_->id(), rowIndex[j] ) ) {
				Eref tgt( e1_, DataId( rowIndex[j] ) );
				if ( tgt.isDataHere() )
					f->op( tgt, arg );
			}
		}
	}
}


/**
 * Should really have a seed argument
 */
unsigned int SparseMsg::randomConnect( double probability )
{
	unsigned int nRows = matrix_.nRows(); // Sources
	unsigned int nCols = matrix_.nColumns();	// Destinations
	matrix_.clear();
	unsigned int totalSynapses = 0;
	unsigned int startSynapse = 0;
	vector< unsigned int > sizes( nCols, 0 );
	bool isFirstRound = 1;
	unsigned int totSynNum = 0;

	// SynElement* syn = dynamic_cast< SynElement* >( e2_ );
	Element* syn = e2_;
	// syn->dataHandler()->getNumData2( sizes );
	// assert( sizes.size() == nCols );
	assert( nCols == syn->dataHandler()->parentDataHandler()->sizeOfDim( 0  ) );
	// assert( nRows == syn->dataHandler()->sizeOfDim( 1 ) );

	vector< unsigned int > dims( syn->dataHandler()->dims() );
	assert( dims.size() > 1 );

	for ( unsigned int i = 0; i < nCols; ++i ) {
		// Check if synapse is on local node
		bool isSynOnMyNode = syn->dataHandler()->isDataHere( i );
		vector< unsigned int > synIndex;
		// This needs to be obtained from current size of syn array.
		// unsigned int synNum = sizes[ i ];
		unsigned int synNum = 0;
		for ( unsigned int j = 0; j < nRows; ++j ) {
			double r = mtrand(); // Want to ensure it is called each time round the loop.
			if ( isSynOnMyNode ) {
				if ( isFirstRound ) {
					startSynapse = totSynNum;
					isFirstRound = 0;
				}
			}
			if ( r < probability && isSynOnMyNode ) {
				synIndex.push_back( synNum );
				++synNum;
			} else {
				synIndex.push_back( ~0 );
			}
			if ( r < probability )
				++totSynNum;
		}
		syn->dataHandler()->setFieldArraySize( i, synNum );
		// sizes[ i ] = synNum;
		totalSynapses += synNum;

		matrix_.addRow( i, synIndex );
	}
	/*
	// Here we figure out the largest # of syns and use it.
	unsigned int biggest = *max_element( sizes.begin(), sizes.end() );
	sizes.resize( 0 );
	sizes.push_back( nCols );
	sizes.push_back( biggest );
	syn->dataHandler()->resize( sizes );
	*/


	// syn->dataHandler()->setNumData2( startSynapse, sizes );
	// cout << Shell::myNode() << ": sizes.size() = " << sizes.size() << ", ncols = " << nCols << ", startSynapse = " << startSynapse << endl;
	matrix_.transpose();
	return totalSynapses;
}

Id SparseMsg::id() const
{
	return id_;
}

void SparseMsg::setMatrix( const SparseMatrix< unsigned int >& m )
{
	matrix_ = m;
}

SparseMatrix< unsigned int >& SparseMsg::getMatrix( )
{
	return matrix_;
}

FullId SparseMsg::findOtherEnd( FullId f ) const
{
	if ( f.id() == e1() ) {
		const unsigned int* entry;
		const unsigned int* colIndex;
		unsigned int num = matrix_.getRow( f.dataId.data(),
			&entry, &colIndex );
		if ( num > 0 ) { // Return the first matching entry.
			return FullId( e2()->id(), DataId( colIndex[0], entry[0] ) );
		}
		return FullId( e2()->id(), DataId::bad() );
	} else if ( f.id() == e2() ) { // Bad! Slow! Avoid!
		vector< unsigned int > entry;
		vector< unsigned int > rowIndex;
		unsigned int num = matrix_.getColumn( f.dataId.data(), 
			entry, rowIndex );
		if ( num > 0 ) { // Return the first matching entry.
			return FullId( e1()->id(), DataId( rowIndex[0], entry[0] ) );
		}
		return FullId( e1()->id(), DataId::bad() );
	}
	return FullId::bad();
}

Msg* SparseMsg::copy( Id origSrc, Id newSrc, Id newTgt,
			FuncId fid, unsigned int b, unsigned int n ) const
{
	const Element* orig = origSrc();
	if ( n <= 1 ) {
		SparseMsg* ret;
		if ( orig == e1() )
			ret = new SparseMsg( Msg::nextMsgId(), newSrc(), newTgt() );
		else if ( orig == e2() )
			ret = new SparseMsg( Msg::nextMsgId(), newTgt(), newSrc() );
		else
			assert( 0 );
		ret->setMatrix( matrix_ );
		ret->numThreads_ = numThreads_;
		ret->nrows_ = nrows_;
		ret->e1()->addMsgAndFunc( ret->mid(), fid, b );
		return ret;
	} else {
		// Here we need a SliceMsg which goes from one 2-d array to another.
		cout << "Error: SparseMsg::copy: SparseSliceMsg not yet implemented\n";
		return 0;
	}
}
