#include "EditLexer.h"
#include "EditStyleX.h"

static KEYWORDLIST Keywords_Probes = { { "break","continue" } };

static EDITSTYLE Styles_Probes[] = {
	EDITSTYLE_DEFAULT,
	{ SCE_NMAPSP_COMMENT_CASE, NP2StyleX_TypeKeyword, L"fore:#0000FF" },
	{ SCE_NMAPSP_COMMENT_CASE_CONTENT, NP2StyleX_Preprocessor, L"fore:#FF8000" },
	{ SCE_NMAPSP_PROBE_SCOPE_BEGIN, NP2StyleX_Type, L"fore:#007F7F" },
	{ SCE_NMAPSP_PROBE, NP2StyleX_Class, L"fore:#0000FF" },
	{ SCE_NMAPSP_PROBE_TYPE, NP2StyleX_Structure, L"fore:#0080FF" },
	{ SCE_NMAPSP_PROBE_BAD, NP2StyleX_Union, L"fore:#EEEEEE" },
	{ SCE_NMAPSP_PROBE_NAME, NP2StyleX_Interface, L"bold; fore:#1E90FF" },
	{ SCE_NMAPSP_PROBE_QUERY, NP2StyleX_Function, L"fore:#A46000" },
	{ SCP_NMAPSP_TEMPLATE_KEY, NP2StyleX_Function, L"fore:#A46000" },
	{ SCE_NMAPSP_PROBE_QUERY_CONTENT, NP2StyleX_Enumeration, L"fore:#FF8000" },
	{ SCP_NMAPSP_TEMPLATE, NP2StyleX_Enumeration, L"fore:#FF8000" },
	{ SCP_NMAPSP_TEMPLATE_FLAG, NP2StyleX_Field, L"fore:#FF80FF" },
	{ SCE_NMAPSP_DEMILITER, NP2StyleX_Constant, L"fore:#B000B0" },
	{ SCP_NMAPSP_TEMPLATE_DEMILITER, NP2StyleX_Constant, L"fore:#B000B0" },
	{ SCE_NMAPSP_COMMENTLINE, NP2StyleX_Comment, L"fore:#608060" },
	{ SCE_NMAPSP_PROBE_QUERY_END, NP2StyleX_DocComment, L"fore:#408040" },
	{ SCP_NMAPSP_KEY, NP2StyleX_Class, L"fore:#0000FF" },
	{ SCP_NMAPSP_MATCH, NP2StyleX_Class, L"fore:#0000FF" },
	{ SCP_NMAPSP_VALUE_BAD, NP2StyleX_Union, L"fore:#EEEEEE" },
	{ SCP_NMAPSP_TEMPLATE_BAD, NP2StyleX_Union, L"fore:#EEEEEE" },
	{ SCP_NMAPSP_VALUE, NP2StyleX_Value, L"bold; fore:#FF0E0E" },
	{ SCP_NMAPSP_SERVICE, NP2StyleX_Value, L"bold; fore:#FF0E0E" },
};

EDITLEXER lexProbes = {
	SCLEX_NMAP_SERVICE_PROBE, NP2LEX_PROBES,
	EDITLEXER_HOLE(L"Nmap Service Probes", Styles_Probes),
	L"probes",
	&Keywords_Probes,
	Styles_Probes
};
