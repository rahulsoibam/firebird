/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		ddl.cpp
 *	DESCRIPTION:	Utilities for generating ddl
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2001.5.20 Claudio Valderrama: Stop null pointer that leads to a crash,
 * caused by incomplete yacc syntax that allows ALTER DOMAIN dom SET;
 *
 * 2001.07.06 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, as the engine now fully supports
 *                         readonly databases.
 * December 2001 Mike Nordell - Attempt to make it C++
 *
 * 2001.5.20 Claudio Valderrama: Stop null pointer that leads to a crash,
 * caused by incomplete yacc syntax that allows ALTER DOMAIN dom SET;
 * 2001.5.29 Claudio Valderrama: Check for view v/s relation in DROP
 * command will stop a user that uses DROP VIEW and drops a table by
 * accident and vice-versa.
 * 2001.5.30 Claudio Valderrama: alter column should use 1..N for the
 * position argument since the call comes from SQL DDL.
 * 2001.6.27 Claudio Valderrama: DDL_resolve_intl_type() was adding 2 to the
 * length of varchars instead of just checking that len+2<=MAX_COLUMN_SIZE.
 * It required a minor change to put_field() where it was decremented, too.
 * 2001.6.27 Claudio Valderrama: Finally stop users from invoking the same option
 * several times when altering a domain. Specially dangerous with text data types.
 * Ex: alter domain d type char(5) type varchar(5) default 'x' default 'y';
 * Bear in mind that if DYN functions are addressed directly, this protection
 * becomes a moot point.
 * 2001.6.30 Claudio Valderrama: revert changes from 2001.6.26 because the code
 * is called from several places and there are more functions, even in metd.c,
 * playing the same nonsense game with the field's length, so it needs more
 * careful examination. For now, the new checks in DYN_MOD should catch most anomalies.
 * 2001.7.3 Claudio Valderrama: fix Firebird Bug #223059 with mismatch between number
 * of declared fields for a VIEW and effective fields in the SELECT statement.
 * 2001.07.22 Claudio Valderrama: minor fixes and improvements.
 * 2001.08.18 Claudio Valderrama: RECREATE PROCEDURE.
 * 2001.10.01 Claudio Valderrama: modify_privilege() should recognize that a ROLE can
 *   now be made an explicit grantee.
 * 2001.10.08 Claudio Valderrama: implement fb_sysflag enum values for autogenerated
 *   non-system triggers so DFW can recognize them easily.
 * 2001.10.26 Claudio Valderrama: added a call to the new METD_drop_function()
 *   in DDL_execute() so the metadata cache for udfs can be refreshed.
 * 2001.12.06 Claudio Valderrama: DDL_resolve_intl_type should calculate field length
 * 2002.08.04 Claudio Valderrama: allow declaring and defining variables at the same time
 * 2002.08.04 Dmitry Yemanov: ALTER VIEW
 * 2002.08.31 Dmitry Yemanov: allowed user-defined index names for PK/FK/UK constraints
 * 2002.09.01 Dmitry Yemanov: RECREATE VIEW
 * 2002.09.12 Nickolay Samofatov: fixed cached metadata errors
 * 2004.01.16 Vlad Horsun: added support for default parameters and
 *   EXECUTE BLOCK statement
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "dyn_consts.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/SysFunction.h"
#include "../common/classes/MetaName.h"
#include "../dsql/dsql.h"
#include "../dsql/node.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/ibase.h"
#include "../jrd/Attachment.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/intl.h"
#include "../jrd/intl_classes.h"
#include "../jrd/jrd.h"
#include "../jrd/flags.h"
#include "../jrd/constants.h"
#include "../dsql/errd_proto.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../dsql/utld_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/thread_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/vio_proto.h"
#include "../yvalve/why_proto.h"
#include "../common/utils_proto.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/DSqlDataTypeUtil.h"
#include "../common/StatusArg.h"

#ifdef DSQL_DEBUG
#include "../common/prett_proto.h"
#endif

using namespace Jrd;
using namespace Dsql;
using namespace Firebird;


static void assign_field_length(dsql_fld*, USHORT);
static void post_607(const Arg::StatusVector& v);


///const int DEFAULT_BLOB_SEGMENT_SIZE = 80; // bytes


// Determine whether ids or names should be referenced when generating blr for fields and relations.
bool DDL_ids(const DsqlCompilerScratch* scratch)
{
	return !scratch->getStatement()->isDdl();
}


//
// See the next function for description. This is only a
// wrapper that sets the last parameter to false to indicate
// we are creating a field, not modifying one.
//
void DDL_resolve_intl_type(DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
	const MetaName& collation_name)
{
	DDL_resolve_intl_type2(dsqlScratch, field, collation_name, false);
}


void DDL_resolve_intl_type2(DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
	const MetaName& collation_name, bool modifying)
{
/**************************************
 *
 *  D D L _ r e s o l v e _ i n t l _ t y p e 2
 *
 **************************************
 *
 * Function

 *	If the field is defined with a character set or collation,
 *	resolve the names to a subtype now.
 *
 *	Also resolve the field length & whatnot.
 *
 *  If the field is being created, it will pick the db-wide charset
 *  and collation if not specified. If the field is being modified,
 *  since we don't allow changes to those attributes, we'll go and
 *  calculate the correct old lenth from the field itself so DYN
 *  can validate the change properly.
 *
 *	For International text fields, this is a good time to calculate
 *	their actual size - when declared they were declared in
 *	lengths of CHARACTERs, not BYTES.
 *
 **************************************/

	if (field->fld_type_of_name.hasData())
	{
		if (field->fld_type_of_table.hasData())
		{
			dsql_rel* relation = METD_get_relation(dsqlScratch->getTransaction(), dsqlScratch,
				field->fld_type_of_table.c_str());
			const dsql_fld* fld = NULL;

			if (relation)
			{
				const MetaName fieldName(field->fld_type_of_name);

				for (fld = relation->rel_fields; fld; fld = fld->fld_next)
				{
					if (fieldName == fld->fld_name)
					{
						field->fld_dimensions = fld->fld_dimensions;
						field->fld_source = fld->fld_source;
						field->fld_length = fld->fld_length;
						field->fld_scale = fld->fld_scale;
						field->fld_sub_type = fld->fld_sub_type;
						field->fld_character_set_id = fld->fld_character_set_id;
						field->fld_collation_id = fld->fld_collation_id;
						field->fld_character_length = fld->fld_character_length;
						field->fld_flags = fld->fld_flags;
						field->fld_dtype = fld->fld_dtype;
						field->fld_seg_length = fld->fld_seg_length;
						break;
					}
				}
			}

			if (!fld)
			{
				// column @1 does not exist in table/view @2
				post_607(Arg::Gds(isc_dyn_column_does_not_exist) <<
						 		Arg::Str(field->fld_type_of_name) <<
								field->fld_type_of_table);
			}
		}
		else
		{
			if (!METD_get_domain(dsqlScratch->getTransaction(), field, field->fld_type_of_name))
			{
				// Specified domain or source field does not exist
				post_607(Arg::Gds(isc_dsql_domain_not_found) << Arg::Str(field->fld_type_of_name));
			}
		}

		if (field->fld_dimensions != 0)
		{
			ERRD_post(Arg::Gds(isc_wish_list) <<
				Arg::Gds(isc_random) <<
				Arg::Str("Usage of domain or TYPE OF COLUMN of array type in PSQL"));
		}
	}

	if ((field->fld_dtype > dtype_any_text) && field->fld_dtype != dtype_blob)
	{
		if (field->fld_character_set || collation_name.hasData() || field->fld_flags & FLD_national)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) << Arg::Gds(isc_collation_requires_text));
		}
		return;
	}

	if (field->fld_dtype == dtype_blob)
	{
		if (field->fld_sub_type_name)
		{
			SSHORT blob_sub_type;
			if (!METD_get_type(dsqlScratch->getTransaction(),
					reinterpret_cast<const dsql_str*>(field->fld_sub_type_name)->str_data,
					"RDB$FIELD_SUB_TYPE", &blob_sub_type))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
						  Arg::Gds(isc_dsql_datatype_err) <<
						  Arg::Gds(isc_dsql_blob_type_unknown) <<
						  		Arg::Str(((dsql_str*) field->fld_sub_type_name)->str_data));
			}
			field->fld_sub_type = blob_sub_type;
		}

		if (field->fld_sub_type > isc_blob_text)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
					  Arg::Gds(isc_subtype_for_internal_use));
		}

		if (field->fld_character_set && (field->fld_sub_type == isc_blob_untyped))
			field->fld_sub_type = isc_blob_text;

		if (field->fld_character_set && (field->fld_sub_type != isc_blob_text))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_requires_text));
		}

		if (collation_name.hasData() && (field->fld_sub_type != isc_blob_text))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_requires_text));
		}

		if (field->fld_sub_type != isc_blob_text)
			return;
	}

	if (field->fld_character_set_id != 0 && collation_name.isEmpty())
	{
		// This field has already been resolved once, and the collation
		// hasn't changed.  Therefore, no need to do it again.
		return;
	}


	if (modifying)
	{
#ifdef DEV_BUILD
		const dsql_rel* relation = dsqlScratch->relation;
#endif
		const dsql_fld* afield = field->fld_next;
		USHORT bpc = 0;

		while (afield)
		{
			// The first test is redundant.
			if (afield != field && afield->fld_relation && afield->fld_name == field->fld_name)
			{
				fb_assert(afield->fld_relation == relation || !relation);
				break;
			}

			afield = afield->fld_next;
		}

		if (afield)
		{
			field->fld_character_set_id = afield->fld_character_set_id;
			bpc = METD_get_charset_bpc(dsqlScratch->getTransaction(), field->fld_character_set_id);
			field->fld_collation_id = afield->fld_collation_id;
			field->fld_ttype = afield->fld_ttype;

			if (afield->fld_flags & FLD_national)
				field->fld_flags |= FLD_national;
			else
				field->fld_flags &= ~FLD_national;

			assign_field_length (field, bpc);
			return;
		}
	}

	if (!(field->fld_character_set || field->fld_character_set_id ||	// set if a domain
		(field->fld_flags & FLD_national)))
	{
		// Attach the database default character set, if not otherwise specified

		const dsql_str* dfl_charset = NULL;

		if (dsqlScratch->getStatement()->isDdl() ||
			(dsqlScratch->flags & (
				DsqlCompilerScratch::FLAG_FUNCTION | DsqlCompilerScratch::FLAG_PROCEDURE |
				DsqlCompilerScratch::FLAG_TRIGGER)))
		{
			dfl_charset = METD_get_default_charset(dsqlScratch->getTransaction());
		}
		else
		{
			USHORT charSet = dsqlScratch->getAttachment()->dbb_attachment->att_charset;
			if (charSet != CS_NONE)
			{
				MetaName charSetName = METD_get_charset_name(dsqlScratch->getTransaction(), charSet);
				dfl_charset = MAKE_string(charSetName.c_str(), charSetName.length());
			}
		}

		if (dfl_charset)
			field->fld_character_set = (dsql_nod*) dfl_charset;
		else
		{
			// If field is not specified with NATIONAL, or CHARACTER SET
			// treat it as a single-byte-per-character field of character set NONE.
			assign_field_length(field, 1);
			field->fld_ttype = 0;

			if (collation_name.isEmpty())
				return;
		}
	}

	const char* charset_name = NULL;

	if (field->fld_flags & FLD_national)
		charset_name = NATIONAL_CHARACTER_SET;
	else if (field->fld_character_set)
		charset_name = ((dsql_str*) field->fld_character_set)->str_data;

	// Find an intlsym for any specified character set name & collation name
	const dsql_intlsym* resolved_type = NULL;

	if (charset_name)
	{
		const dsql_intlsym* resolved_charset =
			METD_get_charset(dsqlScratch->getTransaction(), (USHORT) strlen(charset_name), charset_name);

		// Error code -204 (IBM's DB2 manual) is close enough
		if (!resolved_charset)
		{
			// specified character set not found
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_charset_not_found) << Arg::Str(charset_name));
		}

		field->fld_character_set_id = resolved_charset->intlsym_charset_id;
		resolved_type = resolved_charset;
	}

	if (collation_name.hasData())
	{
		const dsql_intlsym* resolved_collation = METD_get_collation(dsqlScratch->getTransaction(),
			collation_name, field->fld_character_set_id);

		if (!resolved_collation)
		{
			MetaName charSetName;

			if (charset_name)
				charSetName = charset_name;
			else
			{
				charSetName = METD_get_charset_name(dsqlScratch->getTransaction(),
					field->fld_character_set_id);
			}

			// Specified collation not found
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_not_found) << collation_name << charSetName);
		}

		// If both specified, must be for same character set
		// A "literal constant" must be handled (charset as ttype_dynamic)

		resolved_type = resolved_collation;

		if ((field->fld_character_set_id != resolved_type->intlsym_charset_id) &&
			(field->fld_character_set_id != ttype_dynamic))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_not_for_charset) << collation_name);
		}

		field->fld_explicit_collation = true;
	}

	assign_field_length (field, resolved_type->intlsym_bytes_per_char);

	field->fld_ttype = resolved_type->intlsym_ttype;
	field->fld_character_set_id = resolved_type->intlsym_charset_id;
	field->fld_collation_id = resolved_type->intlsym_collate_id;
}


static void assign_field_length(dsql_fld* field, USHORT bytes_per_char)
{
/**************************************
 *
 *  a s s i g n _ f i e l d _ l e n g t h
 *
 **************************************
 *
 * Function
 *  We'll see if the field's length fits in the maximum
 *  allowed field, including charset and space for varchars.
 *  Either we raise an error or assign the field's length.
 *  If the charlen comes as zero, we do nothing, although we
 *  know that DYN, MET and DFW will blindly set field length
 *  to zero if they don't catch charlen or another condition.
 *
 **************************************/

	if (field->fld_character_length)
	{
		ULONG field_length = (ULONG) bytes_per_char * field->fld_character_length;

		if (field->fld_dtype == dtype_varying) {
			field_length += sizeof(USHORT);
		}
		if (field_length > MAX_COLUMN_SIZE)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_imp_exc) <<
					  Arg::Gds(isc_field_name) << Arg::Str(field->fld_name));
		}
		field->fld_length = (USHORT) field_length;
	}

}


void DDL_reset_context_stack(DsqlCompilerScratch* dsqlScratch)
{
/**************************************
 *
 *	D D L _ r e s e t _ c o n t e x t _ s t a c k
 *
 **************************************
 *
 * Function
 *	Get rid of any predefined contexts created
 *	for a view or trigger definition.
 *	Also reset hidden variables.
 *
 **************************************/

	dsqlScratch->context->clear();
	dsqlScratch->contextNumber = 0;
	dsqlScratch->derivedContextNumber = 0;

	dsqlScratch->hiddenVarsNumber = 0;
	dsqlScratch->hiddenVariables.clear();
}


// post very often used error - avoid code duplication
static void post_607(const Arg::StatusVector& v)
{
	Arg::Gds err(isc_sqlerr);
	err << Arg::Num(-607) << Arg::Gds(isc_dsql_command_err);

	err.append(v);
	ERRD_post(err);
}
