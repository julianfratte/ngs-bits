#include "FilterCascade.h"
#include "Exceptions.h"
#include "GeneSet.h"
#include "Helper.h"
#include "Log.h"

/*************************************************** FilterParameter ***************************************************/

FilterParameter::FilterParameter(QString n, FilterParameterType t, QVariant v, QString d)
	: name(n)
	, type(t)
	, value(v)
	, description(d)
{
}


QString FilterParameter::typeAsString(FilterParameterType type)
{
	if (type==INT)
	{
		return "INT";
	}
	else if (type==DOUBLE)
	{
		return "DOUBLE";
	}
	else if (type==BOOL)
	{
		return "BOOL";
	}
	else if (type==STRING)
	{
		return "STRING";
	}
	else if (type==STRINGLIST)
	{
		return "STRINGLIST";
	}
	else
	{
		THROW(ProgrammingException, "Missing type in FilterParameter::typeAsString!");
	}
}


/*************************************************** FilterResult ***************************************************/

FilterResult::FilterResult()
{
}

FilterResult::FilterResult(int variant_count, bool value)
{
	pass = QBitArray(variant_count, value);
}

void FilterResult::removeFlagged(VariantList& variants)
{
	//skip if all variants pass
	if (countPassing()==variants.count()) return;

	//move passing variant to the front of the variant list
	int to_index = 0;
	for (int i=0; i<variants.count(); ++i)
	{
		if (pass[i])
		{
			if (to_index!=i)
			{
				variants[to_index] = variants[i];
			}
			++to_index;
		}
	}

	//resize to new size
	variants.resize(to_index);

	//update flags
	pass = QBitArray(variants.count(), true);
}

void FilterResult::tagNonPassing(VariantList& variants, QByteArray tag, QByteArray description)
{
	//create 'filter' column (if missing)
	int index = variants.addAnnotationIfMissing("filter", "Filter column.");

	//add tag description (if missing)
	if (!variants.filters().contains(tag))
	{
		variants.filters().insert(tag, description);
	}

	//tag variants that did not pass
	for (int i=0; i<variants.count(); ++i)
	{
		if (!pass[i])
		{
			variants[i].addFilter(tag, index);
		}
	}
}

/*************************************************** FilterBase ***************************************************/

FilterBase::FilterBase()
	: name_()
	, description_()
	, params_()
	, enabled_(true)
{
}

FilterBase::~FilterBase()
{
}

QStringList FilterBase::description(bool add_parameter_description) const
{
	QStringList output = description_;
	if (add_parameter_description && params_.count()>0)
	{
		output << "Parameters:";

		foreach(const FilterParameter& p, params_)
		{
			QString text = p.name + " - " + p.description;
			QString default_value = p.type==STRINGLIST ? p.value.toStringList().join(",").trimmed() : p.value.toString().trimmed();
			if (default_value!="")
			{
				text += " [default=" + default_value + "]";
			}
			if (p.type==INT || p.type==DOUBLE)
			{
				if (p.constraints.contains("min"))
				{
					text += " [min=" + p.constraints["min"] + "]";
				}
				if (p.constraints.contains("max"))
				{
					text += " [max=" + p.constraints["max"] + "]";
				}
			}
			else if (p.type==STRING || p.type==STRINGLIST)
			{
				if (p.constraints.contains("valid"))
				{
					text += " [valid=" + p.constraints["valid"] + "]";
				}
				if (p.constraints.contains("not_empty"))
				{
					text += " [non-empty]";
				}
			}
			output << "  " + text;
		}
	}
	return output;
}

void FilterBase::setGeneric(const QString& name, const QString& value)
{
	FilterParameterType type = parameter(name).type;
	if (type==DOUBLE)
	{
		bool ok = false;
		double value_conv = value.toDouble(&ok);
		if (!ok) THROW(ArgumentException, "Could not convert '" + value + "' to double (parameter '" + name + "' of filter '" + this->name() + "')!");

		setDouble(name, value_conv);
	}
	else if (type==INT)
	{
		bool ok = false;
		int value_conv = value.toInt(&ok);
		if (!ok) THROW(ArgumentException, "Could not convert '" + value + "' to integer (parameter '" + name + "' of filter '" + this->name() + "')!");

		setInteger(name, value_conv);
	}
	else if (type==BOOL)
	{
		bool value_conv;
		if (value.toLower()=="yes" || value.toLower()=="true") value_conv = true;
		else if (value.toLower()=="no" || value.toLower()=="false") value_conv = false;
		else THROW(ArgumentException, "Could not convert '" + value + "' to boolean (parameter '" + name + "' of filter '" + this->name() + "')!");

		setBool(name, value_conv);
	}
	else if (type==STRING)
	{
		setString(name, value);
	}
	else if (type==STRINGLIST)
	{
		setStringList(name, value.split(','));
	}
	else
	{
		THROW(ProgrammingException, "Filter parameter type '" + QString(QVariant::typeToName(type)) + "' not supported in setGenericParameter (parameter '" + name + "' of filter '" + this->name() + "')!");
	}
}

void FilterBase::setDouble(const QString& name, double value)
{
	checkParameterType(name, DOUBLE);

	parameter(name).value = value;
}

void FilterBase::setString(const QString& name, const QString& value)
{
	checkParameterType(name, STRING);

	parameter(name).value = value;
}

void FilterBase::setStringList(const QString& name, const QStringList& value)
{
	checkParameterType(name, STRINGLIST);

	parameter(name).value = value;
}

void FilterBase::overrideConstraint(const QString& parameter_name, const QString& constraint_name, const QString& constraint_value)
{
	parameter(parameter_name).constraints[constraint_name] = constraint_value;
}

void FilterBase::setInteger(const QString& name, int value)
{
	checkParameterType(name, INT);

	parameter(name).value = value;
}

void FilterBase::setBool(const QString& name, bool value)
{
	checkParameterType(name, BOOL);

	parameter(name).value = value;
}

FilterParameter& FilterBase::parameter(const QString& name)
{
	for (int i=0; i<params_.count(); ++i)
	{
		if (params_[i].name==name) return params_[i];
	}

	THROW(ArgumentException, "Filter '" + this->name() + "' has no parameter '" + name + "'");
}

const FilterParameter& FilterBase::parameter(const QString& name) const
{
	for (int i=0; i<params_.count(); ++i)
	{
		if (params_[i].name==name) return params_[i];
	}

	THROW(ArgumentException, "Filter '" + this->name() + "' has no parameter '" + name + "'");
}

void FilterBase::checkParameterType(const QString& name, FilterParameterType type) const
{
	const FilterParameter& p = parameter(name);
	if (p.type!=type)
	{
		THROW(ProgrammingException, "Parameter '" + name + "' of filter '" + this->name() + "' used as '" + FilterParameter::typeAsString(type) + "', but has type '" + FilterParameter::typeAsString(p.type) + "!");
	}
}

double FilterBase::getDouble(const QString& name, bool check_constraints) const
{
	checkParameterType(name, DOUBLE);

	const FilterParameter& p = parameter(name);

	//value
	bool ok;
	double value = p.value.toDouble(&ok);
	if (!ok) THROW(ArgumentException, "Could not convert '" + p.value.toString() + "' to double (parameter '" + name + "' of filter '" + this->name() + "')!");

	if (check_constraints)
	{
		if (p.constraints.contains("min") && value < p.constraints["min"].toDouble())
		{
			THROW(ArgumentException, "Double value '" + QString::number(value) + "' smaller than minimum '" + p.constraints["min"] + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
		}
		if (p.constraints.contains("max") && value > p.constraints["max"].toDouble())
		{
			THROW(ArgumentException, "Double value '" + QString::number(value) + "' bigger than maximum '" + p.constraints["max"] + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
		}
	}

	return value;
}

int FilterBase::getInt(const QString& name, bool check_constraints) const
{
	checkParameterType(name, INT);

	const FilterParameter& p = parameter(name);

	//value
	bool ok;
	int value = p.value.toInt(&ok);
	if (!ok) THROW(ArgumentException, "Could not convert '" + p.value.toString() + "' to integer (parameter '" + name + "' of filter '" + this->name() + "')!");

	if (check_constraints)
	{
		if (p.constraints.contains("min") && value < p.constraints["min"].toInt())
		{
			THROW(ArgumentException, "Integer value '" + QString::number(value) + "' smaller than minimum '" + p.constraints["min"] + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
		}
		if (p.constraints.contains("max") && value > p.constraints["max"].toInt())
		{
			THROW(ArgumentException, "Integer value '" + QString::number(value) + "' bigger than maximum '" + p.constraints["max"] + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
		}
	}

	return value;
}

double FilterBase::getBool(const QString& name) const
{
	checkParameterType(name, BOOL);

	const FilterParameter& p = parameter(name);

	return p.value.toBool();
}

QString FilterBase::getString(const QString& name, bool check_constraints) const
{
	checkParameterType(name, STRING);

	const FilterParameter& p = parameter(name);

	QString value = p.value.toString().trimmed();

	if (check_constraints)
	{
		if (p.constraints.contains("valid"))
		{
			QStringList valid = p.constraints["valid"].split(',');
			if (!valid.contains(value))
			{
				THROW(ArgumentException, "String value '" + value + "' not valid. Valid are: '" + valid.join("', '") + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
			}
		}
		if (p.constraints.contains("not_empty"))
		{
			if (value.isEmpty())
			{
				THROW(ArgumentException, "String value '" + value + "' must not be empty! (parameter '" + name + "' of filter '" + this->name() + "')!");
			}
		}
	}

	return value;
}

QStringList FilterBase::getStringList(const QString& name, bool check_constraints) const
{
	checkParameterType(name, STRINGLIST);

	const FilterParameter& p = parameter(name);

	QStringList list = p.value.toStringList();

	if (check_constraints)
	{
		if (p.constraints.contains("valid"))
		{
			QStringList valid = p.constraints["valid"].split(',');
			foreach(QString value, list)
			{
				if (!valid.contains(value))
				{
					THROW(ArgumentException, "String list value '" + value + "' not valid. Valid are: '" + valid.join("', '") + "' (parameter '" + name + "' of filter '" + this->name() + "')!");
				}
			}
		}
		if (p.constraints.contains("not_empty"))
		{
			if (list.join("").isEmpty())
			{
				THROW(ArgumentException, "String list must not be empty! (parameter '" + name + "' of filter '" + this->name() + "')!");
			}
		}
	}

	return list;
}

int FilterBase::annotationColumn(const VariantList& variant_list, const QString& column, bool throw_if_missing) const
{
	int index = variant_list.annotationIndexByName(column, true, false);
	if (throw_if_missing && index==-1)
	{
		THROW(ArgumentException, "Could not determine index of column '" + column + "' for filter '" + name() + "'!");
	}
	return index;
}

void FilterBase::checkIsRegistered() const
{
	if (!FilterFactory::filterNames().contains(name_))
	{
		THROW(ProgrammingException, "Filter '" + name() + "' is not registered!");
	}
}

/*************************************************** FilterCascade ***************************************************/

void FilterCascade::moveUp(int index)
{
	filters_.move(index, index-1);
	errors_.clear();
}

void FilterCascade::moveDown(int index)
{
	filters_.move(index, index+1);
	errors_.clear();
}

FilterResult FilterCascade::apply(const VariantList& variants, bool throw_errors, bool debug_time) const
{
	QTime timer;
	timer.start();

	FilterResult result(variants.count());

	//reset errors
	errors_.fill(QStringList(), filters_.count());

	if (debug_time)
	{
		Log::perf("FilterCascade: Initializing took ", timer);
		timer.start();
	}

	for(int i=0; i<filters_.count(); ++i)
	{
		try
		{
			filters_[i]->apply(variants, result);

			if (debug_time)
			{
				Log::perf("FilterCascade: Filter " + filters_[i]->name() + " took ", timer);
				timer.start();
			}
		}
		catch(const Exception& e)
		{
			errors_[i].append(e.message());
			if (throw_errors)
			{
				throw e;
			}
		}
	}

	return result;
}

QStringList FilterCascade::errors(int index) const
{
	if (errors_.isEmpty())
	{
		return QStringList();
	}

	return errors_[index];
}

/*************************************************** FilterFactory ***************************************************/

QSharedPointer<FilterBase> FilterFactory::create(const QString& name, const QStringList& parameters)
{
	const auto& registry = getRegistry();

	//check that filter is known
	if (!registry.contains(name))
	{
		THROW(ArgumentException, "Filter name '" + name + "' is unknown! Valid filter names are: " + filterNames().join(", "));
	}

	//create filter
	QSharedPointer<FilterBase> filter = QSharedPointer<FilterBase>(registry[name]());

	//set parameters
	foreach(QString param, parameters)
	{
		int index = param.indexOf('=');
		filter->setGeneric(param.left(index), param.mid(index+1));
	}

	return filter;
}

QStringList FilterFactory::filterNames()
{
	return getRegistry().keys();
}

template<typename T> FilterBase* createInstance() { return new T; }

QMap<QString, FilterBase*(*)()> FilterFactory::getRegistry()
{
	static QMap<QString, FilterBase*(*)()> output;

	if (output.isEmpty())
	{
		output["Allele frequency"] = &createInstance<FilterAlleleFrequency>;
		output["Allele frequency (sub-populations)"] = &createInstance<FilterSubpopulationAlleleFrequency>;
		output["Genes"] = &createInstance<FilterGenes>;
		output["Filter column empty"] = &createInstance<FilterFilterColumnEmpty>;
		output["Filter columns"] = &createInstance<FilterFilterColumn>;
		output["SNPs only"] = &createInstance<FilterVariantIsSNP>;
		output["Impact"] = &createInstance<FilterVariantImpact>;
		output["Count NGSD"] = &createInstance<FilterVariantCountNGSD>;
		output["Classification NGSD"] = &createInstance<FilterClassificationNGSD>;
		output["Gene inheritance"] = &createInstance<FilterGeneInheritance>;
		output["Gene constraint"] = &createInstance<FilterGeneConstraint>;
		output["Genotype control"] = &createInstance<FilterGenotypeControl>;
		output["Genotype affected"] = &createInstance<FilterGenotypeAffected>;
		output["Column match"] = &createInstance<FilterColumnMatchRegexp>;
		output["Annotated pathogenic"] = &createInstance<FilterAnnotationPathogenic>;
		output["Predicted pathogenic"] = &createInstance<FilterPredictionPathogenic>;
		output["Text search"] = &createInstance<FilterAnnotationText>;
		output["Variant type"] = &createInstance<FilterVariantType>;
		output["Variant quality"] = &createInstance<FilterVariantQC>;
		output["Trio"] = &createInstance<FilterTrio>;
		output["OMIM genes"] = &createInstance<FilterOMIM>;
		output["Conservedness"] = &createInstance<FilterConservedness>;
		output["Regulatory"] = &createInstance<FilterRegulatory>;
	}

	return output;
}

/*************************************************** concrete filters ***************************************************/

FilterAlleleFrequency::FilterAlleleFrequency()
{
	name_ = "Allele frequency";
	description_ = QStringList() << "Filter based on overall allele frequency given by 1000 Genomes and gnomAD.";
	params_ << FilterParameter("max_af", DOUBLE, 1.0, "Maximum allele frequency in %");
	params_.last().constraints["min"] = "0.0";
	params_.last().constraints["max"] = "100.0";

	checkIsRegistered();
}

QString FilterAlleleFrequency::toText() const
{
	return name() + " &le; " + QString::number(getDouble("max_af", false), 'f', 2) + '%';
}

void FilterAlleleFrequency::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//check parameters
	double max_af = getDouble("max_af")/100.0;

	//get column indices
	int i_1000g = annotationColumn(variants, "1000g");
	int i_gnomad = annotationColumn(variants, "gnomAD");

	//filter
	for(int i=0; i<variants.count(); ++i)
	{
		result.flags()[i] = result.flags()[i]
			&& variants[i].annotations()[i_1000g].toDouble()<=max_af
			&& variants[i].annotations()[i_gnomad].toDouble()<=max_af;
	}
}

FilterGenes::FilterGenes()
{
	name_ = "Genes";
	description_ = QStringList() << "Filter for that preserves a gene set.";
	params_ << FilterParameter("genes", STRINGLIST, QStringList(), "Gene set");
	params_.last().constraints["not_empty"] = "";

	checkIsRegistered();
}

QString FilterGenes::toText() const
{
	return name() + " " + getStringList("genes", false).join(",");
}

void FilterGenes::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	GeneSet genes = GeneSet::createFromStringList(getStringList("genes"));

	//get column indices
	int i_gene = annotationColumn(variants, "gene");

	//filter (text-based)
	if (!genes.join('|').contains("*"))
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = genes.intersectsWith(GeneSet::createFromText(variants[i].annotations()[i_gene], ','));
		}
	}
	else //filter (regexp)
	{
		QRegExp reg(genes.join('|').replace("-", "\\-").replace("*", "[A-Z0-9-]*"));
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			GeneSet var_genes = GeneSet::createFromText(variants[i].annotations()[i_gene], ',');
			bool match_found = false;
			foreach(const QByteArray& var_gene, var_genes)
			{
				if (reg.exactMatch(var_gene))
				{
					match_found = true;
					break;
				}
			}
			result.flags()[i] = match_found;
		}
	}
}


void FilterRegions::apply(const VariantList& variants, const BedFile& regions, FilterResult& result)
{
	//check regions
	if(!regions.isMergedAndSorted())
	{
		THROW(ArgumentException, "Cannot filter variant list by regions that are not merged/sorted!");
	}

	//special case when only one region is contained
	if (regions.count()==1)
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = variants[i].overlapsWith(regions[0]);
		}
		return;
	}

	//general case with many regions
	ChromosomalIndex<BedFile> regions_idx(regions);
	for (int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		const Variant& v = variants[i];
		int index = regions_idx.matchingIndex(v.chr(), v.start(), v.end());
		result.flags()[i] = (index!=-1);
	}
}


FilterFilterColumnEmpty::FilterFilterColumnEmpty()
{
	name_ = "Filter column empty";
	description_ = QStringList() << "Filter that perserves variants which have no entry in the 'filter' column.";

	checkIsRegistered();
}

QString FilterFilterColumnEmpty::toText() const
{
	return name();
}

void FilterFilterColumnEmpty::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		result.flags()[i] = variants[i].filters().isEmpty();
	}
}

FilterVariantIsSNP::FilterVariantIsSNP()
{
	name_ = "SNPs only";
	description_ = QStringList() << "Filter that preserves SNPs and removes all other variant types.";

	checkIsRegistered();
}

QString FilterVariantIsSNP::toText() const
{
	return name();
}

void FilterVariantIsSNP::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		result.flags()[i] = variants[i].isSNV();
	}
}

FilterSubpopulationAlleleFrequency::FilterSubpopulationAlleleFrequency()
{
	name_ = "Allele frequency (sub-populations)";
	description_ = QStringList() << "Filter based on sub-population allele frequency given by ESP and gnomAD.";
	params_ << FilterParameter("max_af", DOUBLE, 1.0, "Maximum allele frequency in %");
	params_.last().constraints["min"] = "0.0";
	params_.last().constraints["max"] = "100.0";

	checkIsRegistered();
}

QString FilterSubpopulationAlleleFrequency::toText() const
{
	return name() + " &le; " + QString::number(getDouble("max_af", false), 'f', 2) + '%';
}

void FilterSubpopulationAlleleFrequency::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//check parameters
	double max_af = getDouble("max_af")/100.0;

	//filter
	int i_gnomad = annotationColumn(variants, "gnomAD_sub");
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		QByteArrayList parts = variants[i].annotations()[i_gnomad].split(',');
		foreach(const QByteArray& part, parts)
		{
			if (part.toDouble()>max_af)
			{
				result.flags()[i] = false;
				break;
			}
		}
	}
}

FilterVariantImpact::FilterVariantImpact()
{
	name_ = "Impact";
	description_ = QStringList() << "Filter based on the variant impact given by VEP." << "For more details see: https://www.ensembl.org/info/genome/variation/prediction/predicted_data.html";

	params_ << FilterParameter("impact", STRINGLIST, QStringList() << "HIGH" << "MODERATE" << "LOW", "Valid impacts");
	params_.last().constraints["valid"] = "HIGH,MODERATE,LOW,MODIFIER";
	params_.last().constraints["not_empty"] = "";

	checkIsRegistered();
}

QString FilterVariantImpact::toText() const
{
	return name() + " " + getStringList("impact", false).join(",");
}

void FilterVariantImpact::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//get column indices
	int i_co_sp = annotationColumn(variants, "coding_and_splicing");

	//prepare impacts list (convert to QByteArray and pad with ":")
	QByteArrayList impacts = getStringList("impact").join(":,:").prepend(":").append(":").toLatin1().split(',');

	//filter
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		bool pass_impact = false;
		foreach(const QByteArray& impact, impacts)
		{
			if (variants[i].annotations()[i_co_sp].contains(impact))
			{
				pass_impact = true;
				break;
			}
		}
		result.flags()[i] = pass_impact;
	}
}


FilterVariantCountNGSD::FilterVariantCountNGSD()
{
	name_ = "Count NGSD";
	description_ = QStringList() << "Filter based on the hom/het occurances of a variant in the NGSD.";
	params_ << FilterParameter("max_count", INT, 20, "Maximum NGSD count");
	params_.last().constraints["min"] = "0";
	params_ << FilterParameter("ignore_genotype", BOOL, false, "If set, all NGSD entries are counted independent of the variant genotype. Otherwise, for homozygous variants only homozygous NGSD entries are counted and for heterozygous variants all NGSD entries are counted.");

	checkIsRegistered();
}

QString FilterVariantCountNGSD::toText() const
{
	return name() + " &le; " + QString::number(getInt("max_count", false)) + (getBool("ignore_genotype") ? " (ignore genotype)" : "");
}

void FilterVariantCountNGSD::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	int max_count = getInt("max_count");

	int i_ihdb_hom = annotationColumn(variants, "NGSD_hom");
	int i_ihdb_het = annotationColumn(variants, "NGSD_het");

	if (getBool("ignore_genotype"))
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = (variants[i].annotations()[i_ihdb_hom].toInt() + variants[i].annotations()[i_ihdb_het].toInt()) <= max_count;
		}
	}
	else
	{
		//get affected column indices
		QList<int> geno_indices = variants.getSampleHeader().sampleColumns(true);
		if (geno_indices.isEmpty()) THROW(ArgumentException, "Cannot apply filter '" + name() + "' to variant list without affected samples!");

		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			bool var_is_hom = false;
			foreach(int index, geno_indices)
			{
				const QByteArray& var_geno = variants[i].annotations()[index];
				if (var_geno=="hom")
				{
					var_is_hom = true;
					break;
				}
				else if (var_geno!="het" && var_geno!="wt" && var_geno!="n/a")
				{
					THROW(ProgrammingException, "Unknown genotype '" + var_geno + "'!");
				}
			}

			result.flags()[i] = (variants[i].annotations()[i_ihdb_hom].toInt() + (var_is_hom ? 0 : variants[i].annotations()[i_ihdb_het].toInt())) <= max_count;
		}
	}
}

FilterFilterColumn::FilterFilterColumn()
{
	name_ = "Filter columns";
	description_ = QStringList() << "Filter based on the entries of the 'filter' column.";

	params_ << FilterParameter("entries", STRINGLIST, QStringList(), "Filter column entries");
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("action", STRING, "REMOVE", "Action to perform");
	params_.last().constraints["valid"] = "KEEP,REMOVE,FILTER";

	checkIsRegistered();
}

QString FilterFilterColumn::toText() const
{
	return name() + " " + getString("action", false) + ": " + getStringList("entries", false).join(",");
}

void FilterFilterColumn::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	entries.clear();
	foreach(const QString& entry, getStringList("entries"))
	{
		entries.append(entry.toLatin1());
	}

	QString action = getString("action");
	if (action=="REMOVE")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = !match(variants[i]);
		}
	}
	else if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
}

bool FilterFilterColumn::match(const Variant& v) const
{
	foreach(const QByteArray& f,  v.filters())
	{
		if (entries.contains(f)) return true;
	}

	return false;
}


FilterClassificationNGSD::FilterClassificationNGSD()
{
	name_ = "Classification NGSD";
	description_ = QStringList() << "Filter for variant classification from NGSD.";

	params_ << FilterParameter("classes", STRINGLIST, QStringList() << "4" << "5", "NGSD classes");
	params_.last().constraints["valid"] = "1,2,3,4,5,M";
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("action", STRING, "KEEP", "Action to perform");
	params_.last().constraints["valid"] = "KEEP,FILTER,REMOVE";

	checkIsRegistered();
}

QString FilterClassificationNGSD::toText() const
{
	return name() + " " + getString("action", false) + ": " + getStringList("classes", false).join(",");
}

void FilterClassificationNGSD::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	classes = getStringList("classes");
	i_class = annotationColumn(variants, "classification");

	QString action = getString("action");
	if (action=="REMOVE")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = !match(variants[i]);
		}
	}
	else if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
}

bool FilterClassificationNGSD::match(const Variant& v) const
{
	QString classification = v.annotations()[i_class].trimmed();
	if (classification.isEmpty()) return false;

	return classes.contains(classification);
}


FilterGeneInheritance::FilterGeneInheritance()
{
	name_ = "Gene inheritance";
	description_ = QStringList() << "Filter based on gene inheritance.";

	params_ << FilterParameter("modes", STRINGLIST, QStringList(), "Inheritance mode(s)");
	params_.last().constraints["valid"] = "AR,AD,XLR,XLD,MT";
	params_.last().constraints["not_empty"] = "";

	checkIsRegistered();
}

QString FilterGeneInheritance::toText() const
{
	return name() + " " + getStringList("modes", false).join(",");
}

void FilterGeneInheritance::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//get column indices
	int i_geneinfo = annotationColumn(variants, "gene_info");
	QStringList modes_passing = getStringList("modes");

	//filter
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		//parse gene_info entry - example: AL627309.1 (inh=n/a pLI=n/a), PRPF31 (inh=AD pLI=0.97), 34P13.14 (inh=n/a pLI=n/a)
		QByteArrayList genes = variants[i].annotations()[i_geneinfo].split(',');
		bool any_gene_passed = false;
		foreach(const QByteArray gene, genes)
		{
			int start = gene.indexOf('(');
			QByteArrayList entries = gene.mid(start+1, gene.length()-start-2).split(' ');
			foreach(const QByteArray& entry, entries)
			{
				if (entry.startsWith("inh="))
				{
					QStringList modes = QString(entry.mid(4)).split('+');
					foreach(const QString& mode, modes)
					{
						if (modes_passing.contains(mode))
						{
							any_gene_passed = true;
						}
					}
				}
			}
		}
		result.flags()[i] = any_gene_passed;
	}
}

FilterGeneConstraint::FilterGeneConstraint()
{
	name_ = "Gene constraint";
	description_ = QStringList() << "Filter based on gene constraint (gnomAD o/e score for LOF variants)." << "Note that gene constraint is most helpful for early-onset severe diseases." << "For details on gnomAD o/e, see https://macarthurlab.org/2018/10/17/gnomad-v2-1/" << "Note: ExAC pLI is deprected and support for backward compatibility with old GSvar files.";

	params_ << FilterParameter("max_oe_lof", DOUBLE, 0.35, "Maximum gnomAD o/e score for LoF variants");
	params_.last().constraints["min"] = "0.0";
	params_.last().constraints["max"] = "1.0";

	params_ << FilterParameter("min_pli", DOUBLE, 0.9, "Minumum ExAC pLI score");
	params_.last().constraints["min"] = "0.0";
	params_.last().constraints["max"] = "1.0";

	checkIsRegistered();
}

QString FilterGeneConstraint::toText() const
{
	return name() + " o/e&le;" + QString::number(getDouble("max_oe_lof", false), 'f', 2) + " (pLI&ge;" + QString::number(getDouble("min_pli", false), 'f', 2) + ")";
}

void FilterGeneConstraint::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//get column indices
	int i_geneinfo = annotationColumn(variants, "gene_info");
	double min_pli = getDouble("min_pli");
	double max_oe_lof = getDouble("max_oe_lof");

	//filter
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		//parse gene_info entry - example: AL627309.1 (inh=n/a pLI=n/a), PRPF31 (inh=AD pLI=0.97), 34P13.14 (inh=n/a pLI=n/a oe_lof=)
		QByteArrayList genes = variants[i].annotations()[i_geneinfo].split(',');
		bool any_gene_passed = false;
		foreach(const QByteArray gene, genes)
		{
			int start = gene.indexOf('(');
			QByteArrayList entries = gene.mid(start+1, gene.length()-start-2).split(' ');
			foreach(const QByteArray& entry, entries)
			{
				if (entry.startsWith("pLI="))
				{
					bool ok;
					double pli = entry.mid(4).toDouble(&ok);
					if (!ok) pli = 0.0; // value 'n/a' > pass
					if (pli>=min_pli)
					{
						any_gene_passed = true;
					}
				}
				if (entry.startsWith("oe_lof="))
				{
					bool ok;
					double oe = entry.mid(7).toDouble(&ok);
					if (!ok) oe = 1.0; // value 'n/a' > pass
					if (oe<=max_oe_lof)
					{
						any_gene_passed = true;
					}
				}
			}
		}
		result.flags()[i] = any_gene_passed;
	}
}


FilterGenotypeControl::FilterGenotypeControl()
{
	name_ = "Genotype control";
	description_ = QStringList() << "Filter for genotype of the 'control' sample(s).";

	params_ << FilterParameter("genotypes", STRINGLIST, QStringList(), "Genotype(s)");
	params_.last().constraints["valid"] = "wt,het,hom,n/a";
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("same_genotype", BOOL, false, "Also check that all 'control' samples have the same genotype.");

	checkIsRegistered();
}

QString FilterGenotypeControl::toText() const
{
	return name() + " " + getStringList("genotypes", false).join(",");
}

void FilterGenotypeControl::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	QStringList genotypes = getStringList("genotypes");
	bool same_genotype = getBool("same_genotype");

	//get control column indices
	QList<int> geno_indices = variants.getSampleHeader().sampleColumns(false);
	if (geno_indices.isEmpty()) THROW(ArgumentException, "Cannot apply filter '" + name() + "' to variant list without control samples!");

	//filter
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		if (same_genotype)
		{
			QByteArray geno_all = checkSameGenotype(geno_indices, variants[i]);
			if (geno_all.isEmpty() || !genotypes.contains(geno_all))
			{
				result.flags()[i] = false;
			}
		}
		else
		{
			foreach(int index, geno_indices)
			{
				QString geno = variants[i].annotations()[index];
				if (!genotypes.contains(geno))
				{
					result.flags()[i] = false;
					break;
				}
			}
		}
	}
}

QByteArray FilterGenotypeControl::checkSameGenotype(const QList<int>& geno_indices, const Variant& v) const
{
	QByteArray geno = v.annotations()[geno_indices[0]];
	for (int i=1; i<geno_indices.count(); ++i)
	{
		if (v.annotations()[geno_indices[i]] != geno)
		{
			return QByteArray();
		}
	}

	return geno;
}

FilterGenotypeAffected::FilterGenotypeAffected()
{
	name_ = "Genotype affected";
	description_ = QStringList() << "Filter for genotype(s) of the 'affected' sample(s)." << "Variants pass if 'affected' samples have the same genotype and the genotype is in the list selected genotype(s).";
	params_ << FilterParameter("genotypes", STRINGLIST, QStringList(), "Genotype(s)");
	params_.last().constraints["valid"] = "wt,het,hom,n/a,comp-het";
	params_.last().constraints["not_empty"] = "";

	checkIsRegistered();
}

QString FilterGenotypeAffected::toText() const
{
	return name() + " " + getStringList("genotypes", false).join(",");
}

void FilterGenotypeAffected::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	QStringList genotypes = getStringList("genotypes");

	//get affected column indices
	QList<int> geno_indices = variants.getSampleHeader().sampleColumns(true);
	if (geno_indices.isEmpty()) THROW(ArgumentException, "Cannot apply filter '" + name() + "' to variant list without affected samples!");


	//filter
	if (!genotypes.contains("comp-het"))
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			QByteArray geno_all = checkSameGenotype(geno_indices, variants[i]);
			if (geno_all.isEmpty() || !genotypes.contains(geno_all))
			{
				result.flags()[i] = false;
			}
		}
	}
	else
	{
		int i_gene = annotationColumn(variants, "gene");

		//(1) filter for all genotypes but comp-het
		//(2) count heterozygous passing variants per gene
		QHash<QByteArray, int> gene_to_het;
		FilterResult result_other(variants.count());
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i])
			{
				result_other.flags()[i] = false;
				continue;
			}

			QByteArray geno_all = checkSameGenotype(geno_indices, variants[i]);
			result_other.flags()[i] = !geno_all.isEmpty() && genotypes.contains(geno_all);

			if (geno_all=="het")
			{
				QList<QByteArray> genes = variants[i].annotations()[i_gene].toUpper().split(',');
				foreach(const QByteArray& gene, genes)
				{
					gene_to_het[gene.trimmed()] += 1;
				}

			}
		}

		//apply combined results from above
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			//other filter pass => pass
			if (result_other.flags()[i]) continue;

			//check for comp-het
			bool pass = false;
			QByteArray geno_all = checkSameGenotype(geno_indices, variants[i]);
			if (geno_all=="het")
			{
				QList<QByteArray> genes = variants[i].annotations()[i_gene].toUpper().split(',');
				foreach(const QByteArray& gene, genes)
				{
					if (gene_to_het[gene.trimmed()]>=2)
					{
						pass = true;
						break;
					}
				}
			}
			result.flags()[i] = pass;
		}
	}
}

QByteArray FilterGenotypeAffected::checkSameGenotype(const QList<int>& geno_indices, const Variant& v) const
{
	QByteArray geno = v.annotations()[geno_indices[0]];
	for (int i=1; i<geno_indices.count(); ++i)
	{
		if (v.annotations()[geno_indices[i]] != geno)
		{
			return QByteArray();
		}
	}

	return geno;
}

FilterColumnMatchRegexp::FilterColumnMatchRegexp()
{
	name_ = "Column match";
	description_ = QStringList() << "Filter that matches the content of a column against a perl-compatible regular expression." << "For details about regular expressions, see http://perldoc.perl.org/perlretut.html";

	params_ << FilterParameter("pattern", STRING, "", "Pattern to match to column");
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("column", STRING, "", "Column to filter");
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("action", STRING, "KEEP", "Action to perform");
	params_.last().constraints["valid"] = "KEEP,FILTER,REMOVE";

	checkIsRegistered();
}

QString FilterColumnMatchRegexp::toText() const
{
	return name() + " " + getString("action", false) + ": " + getString("column", false) + " '" + getString("pattern", false) + "'";
}

void FilterColumnMatchRegexp::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	index = annotationColumn(variants, getString("column"));
	regexp.setPattern(getString("pattern"));

	QString action = getString("action");
	if (action=="REMOVE")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = !match(variants[i]);
		}
	}
	else if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
}

bool FilterColumnMatchRegexp::match(const Variant& v) const
{
	QByteArray content = v.annotations()[index].trimmed();
	return regexp.match(content).hasMatch();
}


FilterAnnotationPathogenic::FilterAnnotationPathogenic()
{
	name_ = "Annotated pathogenic";
	description_ = QStringList() << "Filter that matches variants annotated to be pathogenic by ClinVar or HGMD.";

	params_ << FilterParameter("sources", STRINGLIST, QStringList() << "ClinVar" << "HGMD", "Sources of pathogenicity to use");
	params_.last().constraints["valid"] = "ClinVar,HGMD";
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("also_likely_pathogenic", BOOL, false, "Also consider likely pathogenic variants");
	params_ << FilterParameter("action", STRING, "KEEP", "Action to perform");
	params_.last().constraints["valid"] = "KEEP,FILTER";

	checkIsRegistered();
}

QString FilterAnnotationPathogenic::toText() const
{
	return name() + " " + getString("action", false) + ": " + getStringList("sources", false).join(",") + " " + (getBool("also_likely_pathogenic") ? " (also likely pathogenic)" : "");
}

void FilterAnnotationPathogenic::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	QStringList sources = getStringList("sources");
	also_likely_pathogenic = getBool("also_likely_pathogenic");
	i_clinvar = sources.contains("ClinVar") ? annotationColumn(variants, "ClinVar") : -1;
	i_hgmd = sources.contains("HGMD") ? annotationColumn(variants, "HGMD", false) : -1;

	if (getString("action")=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = annotatedPathogenic(variants[i]);
		}

	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = annotatedPathogenic(variants[i]);
		}
	}
}

bool FilterAnnotationPathogenic::annotatedPathogenic(const Variant& v) const
{
	//ClinVar
	if (i_clinvar!=-1)
	{
		const QByteArray& clinvar = v.annotations()[i_clinvar];
		if (clinvar.contains("[pathogenic"))
		{
			return true;
		}
		if (also_likely_pathogenic && clinvar.contains("[likely pathogenic"))
		{
			return true;
		}
	}

	//HGMD
	if (i_hgmd!=-1)
	{
		const QByteArray& hgmd = v.annotations()[i_hgmd];
		if (hgmd.contains("CLASS=DM")) //matches both "DM" and "DM?"
		{
			if (also_likely_pathogenic)
			{
				return true;
			}
			else if (!hgmd.contains("CLASS=DM?"))
			{
				return true;
			}
		}
	}

	return false;
}

FilterPredictionPathogenic::FilterPredictionPathogenic()
{
	name_ = "Predicted pathogenic";
	description_ = QStringList() << "Filter for variants predicted to be pathogenic." << "Prediction scores included are: phyloP>=1.6, Sift=D, PolyPhen=D, fathmm-MKL>=0.5, CADD>=20 and REVEL>=0.5.";
	params_ << FilterParameter("min", INT, 1, "Minimum number of pathogenic predictions");
	params_.last().constraints["min"] = "1";
	params_ << FilterParameter("action", STRING, "FILTER", "Action to perform");
	params_.last().constraints["valid"] = "KEEP,FILTER";

	checkIsRegistered();
}

QString FilterPredictionPathogenic::toText() const
{
	return name() + " " + getString("action", false) + " &ge; " + QString::number(getInt("min", false));
}

void FilterPredictionPathogenic::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	min = getInt("min");
	i_phylop = annotationColumn(variants, "phyloP");
	i_sift = annotationColumn(variants, "Sift");
	i_polyphen = annotationColumn(variants, "PolyPhen");
	i_fathmm = annotationColumn(variants, "fathmm-MKL");
	i_cadd = annotationColumn(variants, "CADD");
	i_revel = annotationColumn(variants, "REVEL");

	if (getString("action")=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = predictedPathogenic(variants[i]);
		}
	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = predictedPathogenic(variants[i]);
		}
	}
}

bool FilterPredictionPathogenic::predictedPathogenic(const Variant& v) const
{
	int count = 0;

	if (v.annotations()[i_sift].contains("D"))
	{
		++count;
	}

	if ( v.annotations()[i_polyphen].contains("D"))
	{
		++count;
	}

	if (v.annotations()[i_fathmm].contains(","))
	{
		QByteArrayList parts = v.annotations()[i_fathmm].split(',');
		foreach(const QByteArray& part, parts)
		{
			bool ok = true;
			double value = part.toDouble(&ok);
			if (ok && value>=0.5)
			{
				++count;
				break;
			}
		}
	}

	bool ok;
	double value = v.annotations()[i_phylop].toDouble(&ok);
	if (ok && value>=1.6) ++count;


	value = v.annotations()[i_cadd].toDouble(&ok);
	if (ok && value>=20.0) ++count;


	value = v.annotations()[i_revel].toDouble(&ok);
	if (ok && value>=0.5) ++count;

	return count>=min;
}


FilterAnnotationText::FilterAnnotationText()
{
	name_ = "Text search";
	description_ = QStringList() << "Filter for text match in variant annotations." << "The text comparison ignores the case.";
	params_ << FilterParameter("term", STRING, QString(), "Search term");
	params_.last().constraints["not_empty"] = "";
	params_ << FilterParameter("action", STRING, "FILTER", "Action to perform");
	params_.last().constraints["valid"] = "FILTER,KEEP,REMOVE";

	checkIsRegistered();
}

QString FilterAnnotationText::toText() const
{
	return name() + " " + getString("action", false) + " " + getString("term", false);
}

void FilterAnnotationText::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	term = getString("term").toLatin1().trimmed().toLower();

	QString action = getString("action");
	if (action=="REMOVE")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = !match(variants[i]);
		}
	}
	else if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
	else //KEEP
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (result.flags()[i]) continue;

			result.flags()[i] = match(variants[i]);
		}
	}
}

bool FilterAnnotationText::match(const Variant& v) const
{
	foreach(const QByteArray& anno, v.annotations())
	{
		if (anno.toLower().contains(term))
		{
			return true;
		}
	}

	return false;
}

FilterVariantType::FilterVariantType()
{
	name_ = "Variant type";
	description_ = QStringList() << "Filter for variant types as defined by sequence ontology." << "For details see http://www.sequenceontology.org/browser/obob.cgi";
	params_ << FilterParameter("HIGH", STRINGLIST, QStringList() << "frameshift_variant" << "splice_acceptor_variant" << "splice_donor_variant" << "start_lost" << "start_retained_variant" << "stop_gained" << "stop_lost", "High impact variant types");
	params_.last().constraints["valid"] = "frameshift_variant,splice_acceptor_variant,splice_donor_variant,start_lost,start_retained_variant,stop_gained,stop_lost";

	params_ << FilterParameter("MODERATE", STRINGLIST, QStringList() << "inframe_deletion" << "inframe_insertion" << "missense_variant", "Moderate impact variant types");
	params_.last().constraints["valid"] = "inframe_deletion,inframe_insertion,missense_variant";

	params_ << FilterParameter("LOW", STRINGLIST, QStringList() << "splice_region_variant", "Low impact variant types");
	params_.last().constraints["valid"] = "splice_region_variant,stop_retained_variant,synonymous_variant";

	params_ << FilterParameter("MODIFIER", STRINGLIST, QStringList(), "Lowest impact variant types");
	params_.last().constraints["valid"] = "3_prime_UTR_variant,5_prime_UTR_variant,NMD_transcript_variant,downstream_gene_variant,intergenic_variant,intron_variant,mature_miRNA_variant,non_coding_transcript_exon_variant,non_coding_transcript_variant,upstream_gene_variant";

	checkIsRegistered();
}

QString FilterVariantType::toText() const
{
	QStringList selected;
	selected << getStringList("HIGH", false);
	selected << getStringList("MODERATE", false);
	selected << getStringList("LOW", false);
	selected << getStringList("MODIFIER", false);

	return name() + " " + selected.join(",");
}

void FilterVariantType::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	QByteArrayList types;
	foreach(QString type, getStringList("HIGH"))
	{
		types.append(type.trimmed().toLatin1());
	}
	foreach(QString type, getStringList("MODERATE"))
	{
		types.append(type.trimmed().toLatin1());
	}
	foreach(QString type, getStringList("LOW"))
	{
		types.append(type.trimmed().toLatin1());
	}
	foreach(QString type, getStringList("MODIFIER"))
	{
		types.append(type.trimmed().toLatin1());
	}

	int index = annotationColumn(variants, "coding_and_splicing");

	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		bool match_found = false;
		foreach(const QByteArray& type, types)
		{
			if (variants[i].annotations()[index].contains(type))
			{
				match_found = true;
				break;
			}
		}
		result.flags()[i] = match_found;
	}
}


FilterVariantQC::FilterVariantQC()
{
	name_ = "Variant quality";
	description_ = QStringList() << "Filter for variant quality";
	params_ << FilterParameter("qual", INT, 30, "Minimum variant quality score (Phred)");
	params_.last().constraints["min"] = "0";
	params_ << FilterParameter("depth", INT, 20, "Minimum depth");
	params_.last().constraints["min"] = "0";
	params_ << FilterParameter("mapq", INT, 55, "Minimum mapping quality of alternate allele (Phred)");
	params_.last().constraints["min"] = "0";

	checkIsRegistered();
}

QString FilterVariantQC::toText() const
{
	return name() + " qual&ge;" + QString::number(getInt("qual", false)) + " depth&ge;" + QString::number(getInt("depth", false)) + " mapq&ge;" + QString::number(getInt("mapq", false));
}

void FilterVariantQC::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	int index = annotationColumn(variants, "quality");
	int qual = getInt("qual");
	int depth = getInt("depth");
	int mapq = getInt("mapq");

	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;
		QByteArrayList parts = variants[i].annotations()[index].split(';');
		foreach(const QByteArray& part, parts)
		{
			if (part.startsWith("QUAL="))
			{
				if (part.mid(5).toInt()<qual)
				{
					result.flags()[i] = false;
				}
			}
			else if (part.startsWith("DP="))
			{
				if (!part.contains(',')) //single-sample analysis
				{
					if (part.mid(3).toInt()<depth)
					{
						result.flags()[i] = false;
					}
				}
				else //multi-sample analysis (comma-separed depth values)
				{
					QByteArrayList dps = part.mid(3).split(',');
					foreach(const QByteArray& dp, dps)
					{
						if (dp.toInt()<depth)
						{
							result.flags()[i] = false;
						}
					}
				}
			}
			else if (part.startsWith("MQM="))
			{
				if (part.mid(4).toInt()<mapq)
				{
					result.flags()[i] = false;
				}
			}
		}
	}
}


FilterTrio::FilterTrio()
{
	name_ = "Trio";
	description_ = QStringList() << "Filter trio variants";
	params_ << FilterParameter("types", STRINGLIST, QStringList() << "de-novo" << "recessive" << "comp-het" << "LOH" << "x-linked", "Variant types");
	params_.last().constraints["valid"] = "de-novo,recessive,comp-het,LOH,x-linked,imprinting";
	params_.last().constraints["non-empty"] = "";

	params_ << FilterParameter("gender_child", STRING, "n/a", "Gender of the child - if 'n/a', the gender from the GSvar file header is taken");
	params_.last().constraints["valid"] = "male,female,n/a";

	checkIsRegistered();
}

QString FilterTrio::toText() const
{
	return name() + " " + getStringList("types", false).join(',');
}

void FilterTrio::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	//determine child gender
	QString gender_child = getString("gender_child");
	if (gender_child=="n/a")
	{
		gender_child = variants.getSampleHeader(true).infoByStatus(true).gender();
	}
	if (gender_child=="n/a")
	{
		THROW(ArgumentException, "Could not determine gender of child, please set it!");
	}

	//determine column indices
	i_quality = annotationColumn(variants, "quality");
	int i_gene = annotationColumn(variants, "gene");
	SampleHeaderInfo sample_headers = variants.getSampleHeader();
	i_c = sample_headers.infoByStatus(true).column_index;
	i_f = sample_headers.infoByStatus(false, "male").column_index;
	i_m = sample_headers.infoByStatus(false, "female").column_index;

	//determine AF indices
	QList<int> tmp;
	tmp << i_c << i_f << i_m;
	std::sort(tmp.begin(), tmp.end());
	i_af_c = tmp.indexOf(i_c);
	i_af_f = tmp.indexOf(i_f);
	i_af_m = tmp.indexOf(i_m);

	//pre-calculate genes with heterozygous variants
	QSet<QString> types = getStringList("types").toSet();
	GeneSet genes_comphet;
	if (types.contains("comp-het"))
	{
		GeneSet het_father;
		GeneSet het_mother;

		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			const Variant& v = variants[i];

			bool diplod_chromosome = v.chr().isAutosome() || (v.chr().isX() && gender_child=="female");
			if (diplod_chromosome)
			{
				QByteArray geno_c, geno_f, geno_m;
				correctedGenotypes(v, geno_c, geno_f, geno_m);

				if (geno_c=="het" && geno_f=="het" && geno_m=="wt")
				{
					het_mother << GeneSet::createFromText(v.annotations()[i_gene], ',');
				}
				if (geno_c=="het" && geno_f=="wt" && geno_m=="het")
				{
					het_father << GeneSet::createFromText(v.annotations()[i_gene], ',');
				}
			}
		}
		genes_comphet = het_mother.intersect(het_father);
	}

	//load imprinting gene list
	QMap<QByteArray, QByteArray> imprinting;
	if (types.contains("imprinting"))
	{
		QStringList lines = Helper::loadTextFile(":/Resources/imprinting_genes.tsv", true, '#', true);
		foreach(QString line, lines)
		{
			QStringList parts = line.split("\t");
			if (parts.count()==2)
			{
				imprinting[parts[0].toLatin1()] = parts[1].toLatin1();
			}
		}
	}

	//apply
	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		const Variant& v = variants[i];

		//get genotypes
		QByteArray geno_c, geno_f, geno_m;
		correctedGenotypes(v, geno_c, geno_f, geno_m);

		//remove variants where index is wild-type
		if (geno_c=="wt")
		{
			result.flags()[i] = false;
			continue;
		}

		//remove variants where genotype data is missing
		if (geno_c=="n/a" || geno_f=="n/a" || geno_m=="n/a")
		{
			result.flags()[i] = false;
			continue;
		}

		bool diplod_chromosome = v.chr().isAutosome() || (v.chr().isX() && gender_child=="female");

		//filter
		bool match = false;
		if (types.contains("de-novo"))
		{
			if (geno_f=="wt" && geno_m=="wt")
			{
				match = true;
			}
		}
		if (types.contains("recessive"))
		{
			if (diplod_chromosome)
			{
				if (geno_c=="hom" && geno_f=="het" && geno_m=="het")
				{
					match = true;
				}
			}
		}
		if (types.contains("LOH"))
		{
			if (diplod_chromosome)
			{
				if ((geno_c=="hom" && geno_f=="het" && geno_m=="wt") || (geno_c=="hom" && geno_f=="wt" && geno_m=="het"))
				{
					match = true;
				}
			}
		}
		if (types.contains("comp-het"))
		{
			if (diplod_chromosome)
			{
				if ((geno_c=="het" && geno_f=="het" && geno_m=="wt")
					||
					(geno_c=="het" && geno_f=="wt" && geno_m=="het"))
				{
					if (genes_comphet.intersectsWith(GeneSet::createFromText(v.annotations()[i_gene], ',')))
					{
						match = true;
					}
				}
			}
		}
		if (types.contains("x-linked") && v.chr().isX() && gender_child=="male")
		{
			if (geno_c=="hom" && geno_f=="wt" && geno_m=="het")
			{
				match = true;
			}
		}
		if (types.contains("imprinting"))
		{
			if (geno_c=="het" && geno_f=="het" && geno_m=="wt")
			{
				GeneSet genes = GeneSet::createFromText(v.annotations()[i_gene], ',');
				foreach(const QByteArray& gene, genes)
				{
					if (imprinting.contains(gene) && (imprinting[gene]=="paternal" || imprinting[gene]=="both"))
					{
						match = true;
					}
				}
			}
			if (geno_c=="het" && geno_f=="wt" && geno_m=="het")
			{
				GeneSet genes = GeneSet::createFromText(v.annotations()[i_gene], ',');
				foreach(const QByteArray& gene, genes)
				{
					if (imprinting.contains(gene) && (imprinting[gene]=="maternal" || imprinting[gene]=="both"))
					{
						match = true;
					}
				}
			}
		}

		result.flags()[i] = match;
	}
}

void FilterTrio::correctedGenotypes(const Variant& v, QByteArray& geno_c, QByteArray& geno_f, QByteArray& geno_m) const
{
	geno_c = v.annotations()[i_c];
	geno_f = v.annotations()[i_f];
	geno_m = v.annotations()[i_m];

	//correct genotypes based on AF
	QByteArrayList q_parts = v.annotations()[i_quality].split(';');
	foreach(const QByteArray& part, q_parts)
	{
		if (part.startsWith("AF="))
		{
			QByteArrayList af_parts = part.mid(3).split(',');

			if (geno_f=="wt" && af_parts[i_af_f].toDouble()>=0.05 && af_parts[i_af_f].toDouble()<=0.3)
			{
				geno_f = "het";
			}
			if (geno_m=="wt" && af_parts[i_af_m].toDouble()>=0.05 && af_parts[i_af_m].toDouble()<=0.3)
			{
				geno_m = "het";
			}
			if (geno_c=="het" && af_parts[i_af_c].toDouble()<0.1)
			{
				geno_c = "wt";
			}
		}
	}


}

FilterOMIM::FilterOMIM()
{
	name_ = "OMIM genes";
	description_ = QStringList() << "Filter for OMIM genes i.e. the 'OMIM' column is not empty.";
	params_ << FilterParameter("action", STRING, "FILTER", "Action to perform");
	params_.last().constraints["valid"] = "REMOVE,FILTER";
	checkIsRegistered();
}

QString FilterOMIM::toText() const
{
	return name();
}

void FilterOMIM::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	int index = annotationColumn(variants, "OMIM");

	QString action = getString("action");
	if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			if (variants[i].annotations()[index].trimmed().isEmpty())
			{
				result.flags()[i] = false;
			}
		}
	}
	else //REMOVE
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			if (!variants[i].annotations()[index].trimmed().isEmpty())
			{
				result.flags()[i] = false;
			}
		}
	}
}

FilterConservedness::FilterConservedness()
{
	name_ = "Conservedness";
	description_ = QStringList() << "Filter for variants that affect conserved bases";
	params_ << FilterParameter("min_score", DOUBLE, 1.6, "Minimum phlyoP score.");

	checkIsRegistered();
}

QString FilterConservedness::toText() const
{
	return name() + " phyloP&ge;" + QString::number(getDouble("min_score", false));
}

void FilterConservedness::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	int i_phylop = annotationColumn(variants, "phyloP");
	double min_score = getDouble("min_score");

	for(int i=0; i<variants.count(); ++i)
	{
		if (!result.flags()[i]) continue;

		bool ok;
		double value = variants[i].annotations()[i_phylop].toDouble(&ok);
		if (!ok || value<min_score)
		{
			result.flags()[i] = false;
		}
	}
}


FilterRegulatory::FilterRegulatory()
{
	name_ = "Regulatory";
	description_ = QStringList() << "Filter for regulatory variants, i.e. the 'regulatory' column is not empty.";
	params_ << FilterParameter("action", STRING, "FILTER", "Action to perform");
	params_.last().constraints["valid"] = "REMOVE,FILTER";
}

QString FilterRegulatory::toText() const
{
	return name() + " " + getString("action", false);
}

void FilterRegulatory::apply(const VariantList& variants, FilterResult& result) const
{
	if (!enabled_) return;

	int index = annotationColumn(variants, "regulatory");

	QString action = getString("action");
	if (action=="FILTER")
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			if (variants[i].annotations()[index].trimmed().isEmpty())
			{
				result.flags()[i] = false;
			}
		}
	}
	else //REMOVE
	{
		for(int i=0; i<variants.count(); ++i)
		{
			if (!result.flags()[i]) continue;

			if (!variants[i].annotations()[index].trimmed().isEmpty())
			{
				result.flags()[i] = false;
			}
		}
	}
}
