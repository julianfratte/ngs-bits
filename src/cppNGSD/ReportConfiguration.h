#ifndef REPORTCONFIGURATION_H
#define REPORTCONFIGURATION_H

#include "cppNGSD_global.h"
#include "VariantType.h"
#include <QStringList>
#include <QDateTime>

///Variant meta data for report.
struct CPPNGSDSHARED_EXPORT ReportVariantConfiguration
{
	///Default constructor
	ReportVariantConfiguration();
	///Returns if the variant is to be shown in the report
	bool showInReport() const;

	VariantType variant_type;
	int variant_index;

	QString report_type;
	bool causal;
	QString classification;
	QString inheritance;
	bool de_novo;
	bool mosaic;
	bool comp_het;
	bool exclude_artefact; //variant is an artefact
	bool exclude_frequency; //variant frequency is too high in NGSD, gnomAD or elsewhere
	bool exclude_phenotype; //variant/gene does not match genotype
	bool exclude_mechanism; //variant pathomechanism not matching
	bool exclude_other; //other reason
	QString comments; //comments of 1. evaluation
	QString comments2; //comments of 2. evaluation

	//Returns options for 'type' (taken and cached from NGSD)
	static QStringList getTypeOptions(bool test_db = false);
	//Returns options for 'inheritance_mode' (taken and cached from NGSD)
	static QStringList getInheritanceModeOptions(bool test_db = false);
	//Returns options for 'class' (taken and cached from NGSD)
	static QStringList getClassificationOptions(bool test_db = false);
};


///Report configuration
class CPPNGSDSHARED_EXPORT ReportConfiguration
{
public:
	ReportConfiguration();

	///Returns the report configuration for variants
	const QList<ReportVariantConfiguration>& variantConfig() const;
	///Returns indices of the matching variants .
	QList<int> variantIndices(VariantType type, bool only_selected, QString report_type = QString()) const;

	///Returns if a report configuration exists for the variant.
	bool exists(VariantType type, int index) const;
	///Returns the matching report configuration (throws an error if not found).
	const ReportVariantConfiguration& get(VariantType type, int index) const;
	///Sets the report configuration for the variant. Returns if it already existed.
	bool set(const ReportVariantConfiguration& config);
	///Removes the matching configuration. Returns if a configuration was removed.
	bool remove(VariantType type, int index);

	///Returns by who the database entry was created (or last modified).
	QString createdBy() const;
	///Sets the creator name.
	void setCreatedBy(QString user_name);

	///Returns when the database entry was created (or last modified).
	QDateTime createdAt() const;
	///Sets the creation time
	void setCreatedAt(QDateTime time);

	///Returns the number of report config entries (all types).
	int count() const
	{
		return variant_config_.count();
	}

private:
	QList<ReportVariantConfiguration> variant_config_;
	QString created_by_;
	QDateTime created_at_;

	//sort by variant index
	void sortByPosition();
};

#endif // REPORTCONFIGURATION_H
