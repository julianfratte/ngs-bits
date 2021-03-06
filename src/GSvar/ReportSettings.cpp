#include "ReportSettings.h"

ReportSettings::ReportSettings()
	: diag_status()
	, report_config()
	, show_coverage_details(true)
	, min_depth(20)
	, roi_low_cov(false)
	, recalculate_avg_depth(false)
	, show_tool_details(false)
	, show_omim_table(true)
	, show_class_details(false)
	, language("german")
{
}
