/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_WIN_NODES_H
#define DSQL_WIN_NODES_H

#include "../jrd/blr.h"
#include "../dsql/Nodes.h"

namespace Jrd {


// DENSE_RANK function.
class DenseRankWinNode : public WinFuncNode
{
public:
	explicit DenseRankWinNode(MemoryPool& pool);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;
};

// RANK function.
class RankWinNode : public WinFuncNode
{
public:
	explicit RankWinNode(MemoryPool& pool);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual ValueExprNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;

private:
	USHORT tempImpure;
};

// ROW_NUMBER function.
class RowNumberWinNode : public WinFuncNode
{
public:
	explicit RowNumberWinNode(MemoryPool& pool);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

	virtual bool shouldCallWinPass() const
	{
		return true;
	}

	virtual dsc* winPass(thread_db* tdbb, jrd_req* request, SlidingWindow* window) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;
};

// FIRST_VALUE function.
class FirstValueWinNode : public WinFuncNode
{
public:
	explicit FirstValueWinNode(MemoryPool& pool, dsql_nod* aArg = NULL);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

	virtual bool shouldCallWinPass() const
	{
		return true;
	}

	virtual dsc* winPass(thread_db* tdbb, jrd_req* request, SlidingWindow* window) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);
};

// LAST_VALUE function.
class LastValueWinNode : public WinFuncNode
{
public:
	explicit LastValueWinNode(MemoryPool& pool, dsql_nod* aArg = NULL);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

	virtual bool shouldCallWinPass() const
	{
		return true;
	}

	virtual dsc* winPass(thread_db* tdbb, jrd_req* request, SlidingWindow* window) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);
};

// NTH_VALUE function.
class NthValueWinNode : public WinFuncNode
{
public:
	explicit NthValueWinNode(MemoryPool& pool, dsql_nod* aArg = NULL, dsql_nod* aRow = NULL);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

	virtual bool shouldCallWinPass() const
	{
		return true;
	}

	virtual dsc* winPass(thread_db* tdbb, jrd_req* request, SlidingWindow* window) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

private:
	dsql_nod* dsqlRow;
	NestConst<ValueExprNode> row;
};

// LAG/LEAD function.
class LagLeadWinNode : public WinFuncNode
{
public:
	explicit LagLeadWinNode(MemoryPool& pool, const AggInfo& aAggInfo, int aDirection,
		dsql_nod* aArg = NULL, dsql_nod* aRows = NULL, dsql_nod* aOutExpr = NULL);

	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);

	virtual void aggInit(thread_db* tdbb, jrd_req* request) const;
	virtual void aggPass(thread_db* tdbb, jrd_req* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, jrd_req* request) const;

	virtual bool shouldCallWinPass() const
	{
		return true;
	}

	virtual dsc* winPass(thread_db* tdbb, jrd_req* request, SlidingWindow* window) const;

protected:
	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

protected:
	const int direction;
	dsql_nod* dsqlRows;
	dsql_nod* dsqlOutExpr;
	NestConst<ValueExprNode> rows;
	NestConst<ValueExprNode> outExpr;
};

// LAG function.
class LagWinNode : public LagLeadWinNode
{
public:
	explicit LagWinNode(MemoryPool& pool, dsql_nod* aArg = NULL, dsql_nod* aRows = NULL,
		dsql_nod* aOutExpr = NULL);

	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;
};

// LEAD function.
class LeadWinNode : public LagLeadWinNode
{
public:
	explicit LeadWinNode(MemoryPool& pool, dsql_nod* aArg = NULL, dsql_nod* aRows = NULL,
		dsql_nod* aOutExpr = NULL);

	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) const;
};


} // namespace

#endif // DSQL_WIN_NODES_H