#include "Statistics.h"
#include "Exceptions.h"
#include "ChromosomalIndex.h"
#include "Log.h"
#include "cmath"
#include "numeric"
#include "algorithm"
#include "BasicStatistics.h"
#include <QVector>
#include "Pileup.h"
#include "NGSHelper.h"
#include "FastqFileStream.h"
#include "LinePlot.h"
#include "ScatterPlot.h"
#include "BarPlot.h"
#include "Helper.h"
#include "SampleSimilarity.h"
#include <QFileInfo>
#include <QPair>
#include "Histogram.h"
#include "FilterCascade.h"

QCCollection Statistics::variantList(VariantList variants, bool filter)
{
    QCCollection output;

	//filter variants
	if (filter)
	{
		FilterResult filter_result(variants.count());
		FilterFilterColumnEmpty filter;
		filter.apply(variants, filter_result);
		filter_result.removeFlagged(variants);
	}

    //var_total
	output.insert(QCValue("variant count", variants.count(), "Total number of variants in the target region.", "QC:2000013"));

	//var_perc_dbsnp and high-impact variants
	if (variants.count()==0)
    {
		output.insert(QCValue("known variants percentage", "n/a (no variants)", "Percentage of variants that are known polymorphisms in the dbSNP database.", "QC:2000014"));
		output.insert(QCValue("high-impact variants percentage", "n/a (no variants)", "Percentage of variants with high impact on the protein, i.e. stop-gain, stop-loss, frameshift, splice-acceptor or splice-donor variants.", "QC:2000015"));
	}
	else
	{
		int i_csq = variants.annotationIndexByName("CSQ", true, false);
		if (i_csq==-1)
		{
			output.insert(QCValue("known variants percentage", "n/a (CSQ info field missing)", "Percentage of variants that are known polymorphisms in the dbSNP database.", "QC:2000014"));
			output.insert(QCValue("high-impact variants percentage", "n/a (CSQ info field missing)", "Percentage of variants with high impact on the protein, i.e. stop-gain, stop-loss, frameshift, splice-acceptor or splice-donor variants.", "QC:2000015"));
		}
		else
		{
			double dbsnp_count = 0;
			double high_impact_count = 0;
			for(int i=0; i<variants.count(); ++i)
			{
				if (variants[i].annotations().at(i_csq).contains("|rs")) //works without splitting by transcript
				{
					++dbsnp_count;
				}
				if (variants[i].annotations().at(i_csq).contains("|HIGH|")) //works without splitting by transcript
				{
					++high_impact_count;
				}
			}
			output.insert(QCValue("known variants percentage", 100.0*dbsnp_count/variants.count(), "Percentage of variants that are known polymorphisms in the dbSNP database.", "QC:2000014"));
			output.insert(QCValue("high-impact variants percentage", 100.0*high_impact_count/variants.count(), "Percentage of variants with high impact on the protein, i.e. stop-gain, stop-loss, frameshift, splice-acceptor or splice-donor variants.", "QC:2000015"));
		}
	}

	//homozygous variants
    const int i_gt = variants.annotationIndexByName("GT", true, false);
    if (variants.count()!=0 && i_gt!=-1)
    {
        double hom_count = 0;
        for(int i=0; i<variants.count(); ++i)
        {
			QByteArray geno = variants[i].annotations().at(i_gt);
			if (geno=="1/1" || geno=="1|1")
            {
                ++hom_count;
            }
        }
		output.insert(QCValue("homozygous variants percentage", 100.0*hom_count/variants.count(), "Percentage of variants that are called as homozygous.", "QC:2000016"));
    }
    else
    {
		output.insert(QCValue("homozygous variants percentage", "n/a (GT annotation not found, or no variants)", "Percentage of variants that are called as homozygous.", "QC:2000016"));
    }

    //var_perc_indel / var_ti_tv_ratio
    double indel_count = 0;
    double ti_count = 0;
    double tv_count = 0;
    for(int i=0; i<variants.count(); ++i)
    {
        const Variant& var = variants[i];
		if (var.ref().length()>1 || var.obs().length()>1)
        {
            ++indel_count;
        }
        else if ((var.obs()=="A" && var.ref()=="G") || (var.obs()=="G" && var.ref()=="A") || (var.obs()=="T" && var.ref()=="C") || (var.obs()=="C" && var.ref()=="T"))
        {
            ++ti_count;
        }
        else
        {
            ++tv_count;
        }
    }

    if (variants.count()!=0)
    {
		output.insert(QCValue("indel variants percentage", 100.0*indel_count/variants.count(), "Percentage of variants that are insertions/deletions.", "QC:2000017"));
    }
    else
    {
		output.insert(QCValue("indel variants percentage", "n/a (no variants)", "Percentage of variants that are insertions/deletions.", "QC:2000017"));
    }

    if (tv_count!=0)
    {
		output.insert(QCValue("transition/transversion ratio", ti_count/tv_count , "Transition/transversion ratio of SNV variants.", "QC:2000018"));
    }
    else
    {
		output.insert(QCValue("transition/transversion ratio", "n/a (no variants or tansversions)", "Transition/transversion ratio of SNV variants.", "QC:2000018"));
    }

    return output;
}

QCCollection Statistics::mapping(const BedFile& bed_file, const QString& bam_file, int min_mapq)
{
    //check target region is merged/sorted and create index
    if (!bed_file.isMergedAndSorted())
    {
        THROW(ArgumentException, "Merged and sorted BED file required for coverage details statistics!");
    }
    ChromosomalIndex<BedFile> roi_index(bed_file);

    //open BAM file
	BamReader reader(bam_file);

    //create coverage statistics data structure
    long long roi_bases = 0;
	QHash<int, QMap<int, int> > roi_cov;
    for (int i=0; i<bed_file.count(); ++i)
    {
        const BedLine& line = bed_file[i];

		if (!roi_cov.contains(line.chr().num()))
        {
			roi_cov.insert(line.chr().num(), QMap<int, int>());
        }

        for(int p=line.start(); p<=line.end(); ++p)
        {
			roi_cov[line.chr().num()].insert(p, 0);
        }
        roi_bases += line.length();
    }

    //init counts
    int al_total = 0;
    int al_mapped = 0;
    int al_ontarget = 0;
    int al_dup = 0;
    int al_proper_paired = 0;
    double bases_trimmed = 0;
    double bases_mapped = 0;
    double bases_clipped = 0;
    double insert_size_sum = 0;
	Histogram insert_dist(0, 999, 5);
    long long bases_usable = 0;
    int max_length = 0;
    bool paired_end = false;

    //iterate through all alignments
    BamAlignment al;
	while (reader.getNextAlignment(al))
    {
		//skip secondary alignments
		if (al.isSecondaryAlignment()) continue;

        ++al_total;
		max_length = std::max(max_length, al.length());

        //insert size
		if (al.isPaired())
        {
            paired_end = true;

			if (al.isProperPair())
            {
                ++al_proper_paired;
				int insert_size = std::min(abs(al.insertSize()), 999); //cap insert size at 1000
				insert_size_sum += insert_size;
				insert_dist.inc(insert_size, true);
            }
        }

		if (!al.isUnmapped())
        {
            ++al_mapped;

            //calculate soft/hard-clipped bases
			const int start_pos = al.start();
			const int end_pos = al.end();
			bases_mapped += al.length();
			const QList<CigarOp> cigar_data = al.cigarData();
			foreach(const CigarOp& op, cigar_data)
            {
				if (op.Type==BAM_CSOFT_CLIP || op.Type==BAM_CHARD_CLIP)
                {
					bases_clipped += op.Length;
                }
            }

            //calculate usable bases and base-resolution coverage
			const Chromosome& chr = reader.chromosome(al.chromosomeID());
            QVector<int> indices = roi_index.matchingIndices(chr, start_pos, end_pos);
            if (indices.count()!=0)
            {
                ++al_ontarget;

				if (!al.isDuplicate() && al.mappingQuality()>=min_mapq)
                {
                    foreach(int index, indices)
                    {
                        const int ol_start = std::max(bed_file[index].start(), start_pos);
                        const int ol_end = std::min(bed_file[index].end(), end_pos);
						bases_usable += ol_end - ol_start + 1;
						auto it = roi_cov[chr.num()].lowerBound(ol_start);
						auto end = roi_cov[chr.num()].upperBound(ol_end);
						while (it!=end)
						{
							(*it)++;
							++it;
                        }
                    }
                }
            }
        }

        //trimmed bases (this is not entirely correct if the first alignments are all trimmed, but saves the second pass through the data)
		if (al.length()<max_length)
        {
			bases_trimmed += (max_length - al.length());
        }

		if (al.isDuplicate())
        {
            ++al_dup;
        }
	}

	//calculate coverage depth statistics
	double avg_depth = (double) bases_usable / roi_bases;
	int hist_max = 999;
	int hist_step = 5;
	if (avg_depth>500)
	{
		hist_max += 1000;
		hist_step += 5;
	}
	if (avg_depth>1000)
	{
		hist_max += 1000;
		hist_step += 5;
	}
	Histogram depth_dist(0, hist_max, hist_step);
	QHashIterator<int, QMap<int, int> > it(roi_cov);
    while(it.hasNext())
    {
		it.next();
		QMapIterator<int, int> it2(it.value());
        while(it2.hasNext())
        {
			it2.next();
			depth_dist.inc(it2.value(), true);
        }
	}

    //output
    QCCollection output;
	output.insert(QCValue("trimmed base percentage", 100.0 * bases_trimmed / al_total / max_length, "Percentage of bases that were trimmed during to adapter or quality trimming.", "QC:2000019"));
    output.insert(QCValue("clipped base percentage", 100.0 * bases_clipped / bases_mapped, "Percentage of the bases that are soft-clipped or hand-clipped during mapping.", "QC:2000052"));
    output.insert(QCValue("mapped read percentage", 100.0 * al_mapped / al_total, "Percentage of reads that could be mapped to the reference genome.", "QC:2000020"));
	output.insert(QCValue("on-target read percentage", 100.0 * al_ontarget / al_total, "Percentage of reads that could be mapped to the target region.", "QC:2000021"));
    if (paired_end)
    {
		output.insert(QCValue("properly-paired read percentage", 100.0 * al_proper_paired / al_total, "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
		output.insert(QCValue("insert size", insert_size_sum / al_proper_paired, "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    else
    {
		output.insert(QCValue("properly-paired read percentage", "n/a (single end)", "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
		output.insert(QCValue("insert size", "n/a (single end)", "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    if (al_dup==0)
    {
		output.insert(QCValue("duplicate read percentage", "n/a (no duplicates marked or duplicates removed during data analysis)", "Percentage of reads removed because they were duplicates (PCR, optical, etc).", "QC:2000024"));
    }
    else
    {
		output.insert(QCValue("duplicate read percentage", 100.0 * al_dup / al_total, "Percentage of reads removed because they were duplicates (PCR, optical, etc)", "QC:2000024"));
    }
    output.insert(QCValue("bases usable (MB)", (double)bases_usable / 1000000.0, "Bases sequenced that are usable for variant calling (in megabases).", "QC:2000050"));
    output.insert(QCValue("target region read depth", (double)bases_usable / roi_bases, "Average sequencing depth in target region.", "QC:2000025"));

    QVector<int> depths;
    depths << 10 << 20 << 30 << 50 << 100 << 200 << 500;
    QVector<QString> accessions;
	accessions << "QC:2000026" << "QC:2000027" << "QC:2000028" << "QC:2000029" << "QC:2000030" << "QC:2000031" << "QC:2000032";
    for (int i=0; i<depths.count(); ++i)
    {
		double cov_bases = 0.0;
		for (int bin=depth_dist.binIndex(depths[i]); bin<depth_dist.binCount(); ++bin) cov_bases += depth_dist.binValue(bin);
		output.insert(QCValue("target region " + QString::number(depths[i]) + "x percentage", 100.0 * cov_bases / roi_bases, "Percentage of the target region that is covered at least " + QString::number(depths[i]) + "-fold.", accessions[i]));
    }

	//add depth distribtion plot
	LinePlot plot;
	plot.setXLabel("depth of coverage");
	plot.setYLabel("target region [%]");
	plot.setXValues(depth_dist.xCoords());
	plot.addLine(depth_dist.yCoords(true));
	QString plotname = Helper::tempFileName(".png");
	plot.store(plotname);
	output.insert(QCValue::Image("depth distribution plot", plotname, "Depth of coverage distribution plot calculated one the target region.", "QC:2000037"));
	QFile::remove(plotname);

	//add insert size distribution plot
	if (paired_end)
	{
		LinePlot plot2;
		plot2.setXLabel("insert size");
		plot2.setYLabel("reads [%]");
		plot2.setXValues(insert_dist.xCoords());
		plot2.addLine(insert_dist.yCoords(true));

		plotname = Helper::tempFileName(".png");
		plot2.store(plotname);
		output.insert(QCValue::Image("insert size distribution plot", plotname, "Insert size distribution plot.", "QC:2000038"));
		QFile::remove(plotname);
	}

    return output;
}

QCCollection Statistics::mapping_rna(const QString &bam_file, int min_mapq)
{
    //open BAM file
	BamReader reader(bam_file);

    //init counts
    int al_total = 0;
    int al_mapped = 0;
    int al_ontarget = 0;
    int al_dup = 0;
    int al_proper_paired = 0;
    double bases_trimmed = 0;
    double bases_mapped = 0;
    double bases_clipped = 0;
    double insert_size_sum = 0;
    Histogram insert_dist(0, 999, 5);
    long long bases_usable = 0;
    int max_length = 0;
    bool paired_end = false;

    //hash a read until its paired read occurs
	QMap<QString, QPair<QList<CigarOp>, int>> read_hash;

	//iterate through all alignments
	int last_chr_id = -1;
	BamAlignment al;
	while (reader.getNextAlignment(al))
    {
        //skip secondary alignments
		if (al.isSecondaryAlignment()) continue;

        //empty hash if new reference sequence (chromosome) started
		if (al.chromosomeID() != last_chr_id)
		{
			read_hash.clear();
        }
		last_chr_id = al.chromosomeID();

        ++al_total;
		max_length = std::max(max_length, al.length());

        //insert size
		if (al.isPaired())
        {
            paired_end = true;
			if (al.isProperPair())
            {
                ++al_proper_paired;

				int insert_size = abs(al.insertSize());

                //is the the paired read already present in the hash?
				QString key = al.name();
                auto search_result = read_hash.find(key);
				if(search_result == read_hash.end())
				{
					read_hash.insert(key, qMakePair(al.cigarData(), al.start()-1));
				}
				else
				{
					//compute the insert size using information of both reads
					int start1 = search_result->second;                             // Start pos read1
					int start2 = al.start()-1;                                      // Start pos read2
                    int end1 = start1;                                              // End pos read1
                    int end2 = start2;                                              // End pos read2

					//sweep over read1 and substract the introns from the insert size
					foreach(const CigarOp& op, search_result->first)
					{
						end1 += op.Length;

                        // If the read spans an intron, decrease the insert size
						if(op.Type==BAM_CREF_SKIP)
						{
                            insert_size -= op.Length;
                        }

                        // Stop if read2 was reached
						if(end1 >= start2) break;
                    }

                    //sweep over read2 and substract the introns that starts after read1's end
					QList<CigarOp> cigar2 = al.cigarData();
					foreach(const CigarOp& op, cigar2)
					{
                        // Do not consider parts that were fully overlapped by read1
						if(end2 + (int)op.Length < end1)
						{
                            end2 += op.Length;
                            continue;
                        }

                        end2 += op.Length;

                        // If the read spans an intron, decrease the insert size
						if(op.Type==BAM_CREF_SKIP)
						{
                            insert_size -= op.Length;
                        }
                    }

                    insert_size = std::min(insert_size, 999); // cap insert size at 1000
                    insert_size_sum += 2 * insert_size;     // Twice because the sum is divided by every read of pairs
                    insert_dist.inc(insert_size, true);

                    // The hashed read1 is not needed any more
                    read_hash.erase(search_result);
                }
            }
        }

		if (!al.isUnmapped())
        {
            ++al_mapped;

            //calculate soft/hard-clipped bases
			bases_mapped += al.length();
			const QList<CigarOp> cigar_data = al.cigarData();
			foreach(const CigarOp& op, cigar_data)
            {
				if (op.Type==BAM_CSOFT_CLIP || op.Type==BAM_CHARD_CLIP)
                {
					bases_clipped += op.Length;
                }
            }

            //usable
			if (reader.chromosome(al.chromosomeID()).isNonSpecial())
            {
                ++al_ontarget;

				if (!al.isDuplicate() && al.mappingQuality()>=min_mapq)
                {
					bases_usable += al.length();
                }
            }
        }

        //trimmed bases (this is not entirely correct if the first alignments are all trimmed, but saves the second pass through the data)
		if (al.length()<max_length)
        {
			bases_trimmed += (max_length - al.length());
        }

		if (al.isDuplicate())
        {
            ++al_dup;
        }
	}

    //output
    QCCollection output;
    output.insert(QCValue("trimmed base percentage", 100.0 * bases_trimmed / al_total / max_length, "Percentage of bases that were trimmed during to adapter or quality trimming.", "QC:2000019"));
    output.insert(QCValue("clipped base percentage", 100.0 * bases_clipped / bases_mapped, "Percentage of the bases that are soft-clipped or hand-clipped during mapping.", "QC:2000052"));
    output.insert(QCValue("mapped read percentage", 100.0 * al_mapped / al_total, "Percentage of reads that could be mapped to the reference genome.", "QC:2000020"));
    output.insert(QCValue("on-target read percentage", 100.0 * al_ontarget / al_total, "Percentage of reads that could be mapped to the target region.", "QC:2000021"));
    if (paired_end)
    {
        output.insert(QCValue("properly-paired read percentage", 100.0 * al_proper_paired / al_total, "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
        output.insert(QCValue("insert size", insert_size_sum / al_proper_paired, "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    else
    {
        output.insert(QCValue("properly-paired read percentage", "n/a (single end)", "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
        output.insert(QCValue("insert size", "n/a (single end)", "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    if (al_dup==0)
    {
        output.insert(QCValue("duplicate read percentage", "n/a (duplicates not marked or removed during data analysis)", "Percentage of reads removed because they were duplicates (PCR, optical, etc).", "QC:2000024"));
    }
    else
    {
        output.insert(QCValue("duplicate read percentage", 100.0 * al_dup / al_total, "Percentage of reads removed because they were duplicates (PCR, optical, etc).", "QC:2000024"));
    }
    output.insert(QCValue("bases usable (MB)", (double)bases_usable / 1000000.0, "Bases sequenced that are usable for variant calling (in megabases).", "QC:2000050"));

    //add insert size distribution plot
    if (paired_end)
    {
        LinePlot plot2;
        plot2.setXLabel("insert size");
        plot2.setYLabel("reads [%]");
        plot2.setXValues(insert_dist.xCoords());
        plot2.addLine(insert_dist.yCoords(true));

        QString plotname = Helper::tempFileName(".png");
        plot2.store(plotname);
        output.insert(QCValue::Image("insert size distribution plot", plotname, "Insert size distribution plot.", "QC:2000038"));
        QFile::remove(plotname);
    }

    return output;
}

QCCollection Statistics::mapping(const QString &bam_file, int min_mapq)
{
    //open BAM file
	BamReader reader(bam_file);

    //init counts
    long long al_total = 0;
    long long al_mapped = 0;
    long long al_ontarget = 0;
    long long al_dup = 0;
    long long al_proper_paired = 0;
    double bases_trimmed = 0;
    double bases_mapped = 0;
    double bases_clipped = 0;
    double insert_size_sum = 0;
	Histogram insert_dist(0, 999, 5);
    long long bases_usable = 0;
    int max_length = 0;
    bool paired_end = false;

    //iterate through all alignments
    BamAlignment al;
	while (reader.getNextAlignment(al))
	{
		//skip secondary alignments
		if (al.isSecondaryAlignment()) continue;

        ++al_total;
		max_length = std::max(max_length, al.length());

        //insert size
		if (al.isPaired())
        {
            paired_end = true;

			if (al.isProperPair())
            {
				++al_proper_paired;
				const int insert_size = std::min(abs(al.insertSize()),  999); //cap insert size at 1000
				insert_size_sum += insert_size;
				insert_dist.inc(insert_size, true);
            }
        }

		if (!al.isUnmapped())
        {
            ++al_mapped;

            //calculate soft/hard-clipped bases
			bases_mapped += al.length();
			const QList<CigarOp> cigar_data = al.cigarData();
			foreach(const CigarOp& op, cigar_data)
			{
				if (op.Type==BAM_CSOFT_CLIP || op.Type==BAM_CHARD_CLIP)
				{
					bases_clipped += op.Length;
				}
			}

            //usable
			if (reader.chromosome(al.chromosomeID()).isNonSpecial())
            {
                ++al_ontarget;

				if (!al.isDuplicate() && al.mappingQuality()>=min_mapq)
				{
					bases_usable += al.length();
                }
            }
        }

        //trimmed bases (this is not entirely correct if the first alignments are all trimmed, but saves the second pass through the data)
		if (al.length()<max_length)
        {
			bases_trimmed += (max_length - al.length());
        }

		if (al.isDuplicate())
        {
            ++al_dup;
        }
	}

    //output
    QCCollection output;
	output.insert(QCValue("trimmed base percentage", 100.0 * bases_trimmed / al_total / max_length, "Percentage of bases that were trimmed during to adapter or quality trimming.", "QC:2000019"));
    output.insert(QCValue("clipped base percentage", 100.0 * bases_clipped / bases_mapped, "Percentage of the bases that are soft-clipped or hand-clipped during mapping.", "QC:2000052"));
	output.insert(QCValue("mapped read percentage", 100.0 * al_mapped / al_total, "Percentage of reads that could be mapped to the reference genome.", "QC:2000020"));
	output.insert(QCValue("on-target read percentage", 100.0 * al_ontarget / al_total, "Percentage of reads that could be mapped to the target region.", "QC:2000021"));
    if (paired_end)
    {
		output.insert(QCValue("properly-paired read percentage", 100.0 * al_proper_paired / al_total, "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
		output.insert(QCValue("insert size", insert_size_sum / al_proper_paired, "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    else
    {
		output.insert(QCValue("properly-paired read percentage", "n/a (single end)", "Percentage of properly paired reads (for paired-end reads only).", "QC:2000022"));
		output.insert(QCValue("insert size", "n/a (single end)", "Mean insert size (for paired-end reads only).", "QC:2000023"));
    }
    if (al_dup==0)
    {
		output.insert(QCValue("duplicate read percentage", "n/a (duplicates not marked or removed during data analysis)", "Percentage of reads removed because they were duplicates (PCR, optical, etc).", "QC:2000024"));
    }
    else
    {
		output.insert(QCValue("duplicate read percentage", 100.0 * al_dup / al_total, "Percentage of reads removed because they were duplicates (PCR, optical, etc).", "QC:2000024"));
    }
    output.insert(QCValue("bases usable (MB)", (double)bases_usable / 1000000.0, "Bases sequenced that are usable for variant calling (in megabases).", "QC:2000050"));
	output.insert(QCValue("target region read depth", (double) bases_usable / reader.genomeSize(true), "Average sequencing depth in target region.", "QC:2000025"));

	//add insert size distribution plot
	if (paired_end)
	{
		if (insert_dist.binSum()>0)
		{
			LinePlot plot2;
			plot2.setXLabel("insert size");
			plot2.setYLabel("reads [%]");
			plot2.setXValues(insert_dist.xCoords());
			plot2.addLine(insert_dist.yCoords(true));

			QString plotname = Helper::tempFileName(".png");
			plot2.store(plotname);
			output.insert(QCValue::Image("insert size distribution plot", plotname, "Insert size distribution plot.", "QC:2000038"));
			QFile::remove(plotname);
		}
		else
		{
			Log::warn("Skipping insert size histogram - no read pairs found!");
		}
	}

    return output;
}

QCCollection Statistics::region(const BedFile& bed_file, bool merge)
{
    //sort if necessary
    BedFile regions = bed_file;
    bool is_sorted = regions.isSorted();

    //merge if necessary
    bool is_merged = regions.isMerged();
    if (!is_merged && merge)
    {
        regions.merge();
        is_merged = true;
        is_sorted = true;
    }

    //traverse lines
    QSet<Chromosome> chromosomes;
    QVector<double> lengths;
    int length_min = std::numeric_limits<int>::max();
    int length_max = std::numeric_limits<int>::min();
    lengths.reserve(regions.count());
    for (int i=0; i<regions.count(); ++i)
    {
        const BedLine& line = regions[i];
        chromosomes.insert(line.chr());
        int length = line.length();
        length_min = std::min(length, length_min);
        length_max = std::max(length, length_max);
        lengths.append(length);
    }

    //length statistics
    double length_sum = std::accumulate(lengths.begin(), lengths.end(), 0.0);
    double length_mean = length_sum / lengths.size();
    double sq_sum = std::inner_product(lengths.begin(), lengths.end(), lengths.begin(), 0.0);
    double length_stdev = std::sqrt(sq_sum / lengths.size() - length_mean * length_mean);

    //chromosome list string
    QList<Chromosome> chr_list = chromosomes.toList();
    std::sort(chr_list.begin(), chr_list.end());
    QString chr_list_str = "";
    foreach(Chromosome chr, chr_list)
    {
        if (chr_list_str!="") chr_list_str += ", ";
        chr_list_str += chr.strNormalized(false);
    }

    //output
    QCCollection output;
    output.insert(QCValue("roi_bases", length_sum, "Number of bases in the (merged) target region."));
    output.insert(QCValue("roi_fragments", regions.count(), "Number of (merged) target regions."));
    output.insert(QCValue("roi_chromosomes", QString::number(chromosomes.count()) + " (" + chr_list_str + ")", "Chromosomes in the target region."));
	output.insert(QCValue("roi_is_sorted", is_sorted ? "yes" : "no", "If the target region is sorted according to chromosome and start position."));
	output.insert(QCValue("roi_is_merged", is_merged ? "yes" : "no", "If the target region is merged, i.e. it has no overlapping fragments."));
    output.insert(QCValue("roi_fragment_min", length_min, "Minimum fragment size of (merged) target region."));
    output.insert(QCValue("roi_fragment_max", length_max, "Maximum fragment size of (merged) target region."));
    output.insert(QCValue("roi_fragment_mean", length_mean, "Mean fragment size of (merged) target region."));
    output.insert(QCValue("roi_fragment_stdev", length_stdev, "Fragment size standard deviation of (merged) target region."));
    return output;
}

QCValue Statistics::mutationBurden(QString somatic_vcf, QString exons, QString target, QString tsg, QString blacklist)
{
	QString qcml_name = "somatic variant rate";
	QString qcml_desc = "Categorized somatic variant rate followed by the somatic variant rate [variants/Mbp] normalized for the target region and corrected for tumor suppressors.";
	QString qcml_id = "QC:2000053";
	QCValue undefined = QCValue(qcml_name, "n/a", qcml_desc, qcml_id);

	//check input files given
	if(exons.isEmpty() || target.isEmpty() || tsg.isEmpty() || blacklist.isEmpty())
	{
		return undefined;
	}

	//check input files not empty
	BedFile target_file;
	target_file.load(target);

	BedFile target_exon_file;
	target_exon_file.load(exons);
	double exome_size = target_exon_file.baseCount() / 1000000.0;

	BedFile blacklist_file;
	blacklist_file.load(blacklist);

	BedFile tsg_bed_file;
	tsg_bed_file.load(tsg);

	if(target_file.count()==0 || target_exon_file.count()==0 || blacklist_file.count()==0 || tsg_bed_file.count()==0)
	{
		return undefined;
	}

	//Reduce target region to exons only, we will consider only exonic variants
	target_exon_file.merge();
	target_file.intersect(target_exon_file);

	//Remove blacklisted region from target region
	blacklist_file.merge();
	target_file.subtract(blacklist_file);

	//Check target is still non-empty
	if (target_file.count()==0) return undefined;

	//Process variants
	VariantList variants;
	variants.load(somatic_vcf);
	int somatic_var_count = 0;
	int somatic_count_in_tsg = 0;
	for(int i=0;i<variants.count();++i)
	{
		if(variants[i].filters().contains("freq-nor")) continue;
		if(variants[i].filters().contains("freq-tum")) continue;
		if(variants[i].filters().contains("depth-nor")) continue;
		if(variants[i].filters().contains("depth-tum")) continue;
		if(variants[i].filters().contains("lt-3-reads")) continue;

		const Chromosome chr = variants[i].chr();
		int start = variants[i].start();
		int end = variants[i].end();

		if(target_file.overlapsWith(chr, start, end))
		{
			++somatic_var_count;

			if (tsg_bed_file.overlapsWith(chr, start, end))
			{
				++somatic_count_in_tsg;
			}
		}
	}

	//Calculate mutation burden
	double target_size = target_file.baseCount() / 1000000.0;
	double mutation_burden = ( (somatic_var_count - somatic_count_in_tsg) * exome_size / target_size + somatic_count_in_tsg ) / exome_size;
	return QCValue(qcml_name, QString::number(mutation_burden, 'f', 2), qcml_desc, qcml_id);
}

QCCollection Statistics::somatic(QString build, QString& tumor_bam, QString& normal_bam, QString& somatic_vcf, QString ref_fasta, const BedFile& target_file,bool skip_plots)
{
	QCCollection output;

	//sample correlation
	auto tumor_genotypes = SampleSimilarity::genotypesFromBam(build, tumor_bam, 30, 500, true, &target_file);
	auto normal_genotypes = SampleSimilarity::genotypesFromBam(build, tumor_bam, 30, 500, true, &target_file);
	SampleSimilarity sc;
	sc.calculateSimilarity(tumor_genotypes, normal_genotypes);
	output.insert(QCValue("sample correlation", ( sc.olCount()<100 ? "n/a (too few variants)" : QString::number(sc.sampleCorrelation(),'f',2) ), "SNP-based sample correlation of tumor / normal.", "QC:2000040"));

	//variants
	VariantList variants;
	variants.load(somatic_vcf);
	variants.sort();

	//total variants
	output.insert(QCValue("variant count", variants.count(), "Total number of variants in the target region.", "QC:2000013"));

	//total variants filtered
	int somatic_count = 0;
	for(int i=0; i<variants.count(); ++i)
	{
		if (!variants[i].filters().empty())	continue;
		++somatic_count;
	}
	output.insert(QCValue("somatic variant count", somatic_count, "Total number of somatic variants in the target region.", "QC:2000041"));

	//percentage known variants
	double known_count = 0;
	int i_csq_gnomad = variants.vepIndexByName("gnomAD_AF", false);
	if(i_csq_gnomad!=-1)
	{
		if (variants.count()!=0)
		{
			int i_csq = variants.annotationIndexByName("CSQ");
			for(int i=0; i<variants.count(); ++i)
			{
				if (!variants[i].filters().empty())	continue;

				bool is_known = false;
				QByteArrayList annos = variants[i].vepAnnotations(i_csq, i_csq_gnomad);
				foreach (const QByteArray& anno, annos)
				{
					if (anno.toDouble()>0.01)
					{
						is_known = true;
						break;
					}
				}
				if (is_known)
				{
					++known_count;
				}
			}
			output.insert(QCValue("known somatic variants percentage", 100.0*known_count/somatic_count, "Percentage of somatic variants that are listed as germline variants in public datbases (e.g. AF>1% in ExAC).", "QC:2000045"));
		}
		else
		{
			output.insert(QCValue("known somatic variants percentage", "n/a (no somatic variants)", "Percentage of somatic variants that are listed as germline variants in public datbases (e.g. AF>1% in ExAC).", "QC:2000045"));
		}
	}
	else
	{
		output.insert(QCValue("known somatic variants percentage", "n/a (no gnomAD_AF annotation in CSQ info field)", "Percentage of somatic variants that are listed as germline variants in public datbases (e.g. AF>1% in ExAC).", "QC:2000045"));
	}
	

	//var_perc_indel / var_ti_tv_ratio
	double indel_count = 0;
	double ti_count = 0;
	double tv_count = 0;
	for(int i=0; i<variants.count(); ++i)
	{
		if (!variants[i].filters().empty())	continue;

		const Variant& var = variants[i];
		if (var.ref().length()>1 || var.obs().length()>1)
		{
			++indel_count;
		}
		else if ((var.obs()=="A" && var.ref()=="G") || (var.obs()=="G" && var.ref()=="A") || (var.obs()=="T" && var.ref()=="C") || (var.obs()=="C" && var.ref()=="T"))
		{
			++ti_count;
		}
		else
		{
			++tv_count;
		}
	}
	if (somatic_count!=0)
	{
		output.insert(QCValue("somatic indel percentage", 100.0*indel_count/somatic_count, "Percentage of somatic variants that are insertions/deletions.", "QC:2000042"));
	}
	else
	{
		output.insert(QCValue("somatic indel variants percentage", "n/a (no variants)", "Percentage of variants that are insertions/deletions.", "QC:2000042"));
	}
	if (tv_count!=0)
	{
		output.insert(QCValue("somatic transition/transversion ratio", ti_count/tv_count , "Somatic Transition/transversion ratio of SNV variants.", "QC:2000043"));
	}
	else
	{
		output.insert(QCValue("somatic transition/transversion ratio", "n/a (no variants or transversions)", "Somatic transition/transversion ratio of SNV variants.", "QC:2000043"));
	}


	//estimate tumor content
	int min_depth = 30;
	double max_somatic = 0.01;
	int n = 10;
	//process variants
	QVector<double> freqs;
	BamReader reader_tumor(tumor_bam);
	BamReader reader_normal(normal_bam);
	for (int i=0; i<variants.count(); ++i)
	{
		const Variant& v = variants[i];

		if (!v.isSNV()) continue;
		if (!v.chr().isAutosome()) continue;
		if(!variants[i].filters().empty())	continue;	//skip non-somatic variants

		Pileup pileup_tu = reader_tumor.getPileup(v.chr(), v.start());
		if (pileup_tu.depth(true) < min_depth) continue;
		Pileup pileup_no = reader_normal.getPileup(v.chr(), v.start());
		if (pileup_no.depth(true) < min_depth) continue;

		double no_freq = pileup_no.frequency(v.ref()[0], v.obs()[0]);
		if (!BasicStatistics::isValidFloat(no_freq) || no_freq >= max_somatic) continue;

		double tu_freq = pileup_tu.frequency(v.ref()[0], v.obs()[0]);
		if (!BasicStatistics::isValidFloat(tu_freq) || tu_freq > 0.6) continue;

		freqs.append(tu_freq);
	}

	//sort data
	std::sort(freqs.begin(), freqs.end());

	//print tumor content estimate
	QString value = "";
	if (freqs.count()>=n)
	{
		freqs = freqs.mid(freqs.count()-n);
		value = QString::number(BasicStatistics::median(freqs, false)*200, 'f', 2);
	}
	else
	{
		value = "n/a (too few variants)";
	}
	output.insert(QCValue("tumor content estimate", value, "Estimate of tumor content.", "QC:2000054"));

	if(skip_plots)	return output;

	//plotting
	QString tumor_id = QFileInfo(tumor_bam).baseName();
	QString normal_id = QFileInfo(normal_bam).baseName();
	QStringList nuc = QStringList{"A","C","G","T"};

	if(!variants.sampleExists(tumor_id))	Log::error("Tumor sample " + tumor_id + " was not found in variant file " + somatic_vcf);
	if(!variants.sampleExists(normal_id))	Log::error("Normal sample " + normal_id + " was not found in variant file " + somatic_vcf);

	//plot0: histogram allele frequencies somatic mutations
	Histogram hist_filtered(0,1,0.0125);
	Histogram hist_all(0,1,0.0125);
	for(int i=0; i<variants.count(); ++i)
	{
		if(!variants[i].isSNV())	continue;	//skip indels

		//strelka SNV tumor and normal
		int idx_strelka_snv = variants.annotationIndexByName("AU", tumor_id, true, false);
		if( idx_strelka_snv!=-1 && !variants[i].annotations()[idx_strelka_snv].isEmpty() )
		{
			int count_mut = 0;
			int count_all = 0;
			foreach(QString n, nuc)
			{
				int index_n = variants.annotationIndexByName((n+"U"), tumor_id);
				int tmp = variants[i].annotations()[index_n].split(',')[0].toInt();
				if(n==variants[i].obs())	count_mut += tmp;
				count_all += tmp;
			}
			if(count_all>0)
			{
				hist_all.inc((double)count_mut/count_all);
				if(variants[i].filters().empty())	hist_filtered.inc((double)count_mut/count_all);
			}
		}
		//freebayes tumor and normal
		//##FORMAT=<ID=RO,Number=1,Type=Integer,Description="Reference allele observation count">
		//##FORMAT=<ID=AO,RONumber=A,Type=Integer,Description="Alternate allele observation count">
		else if(variants.annotationIndexByName("AO", tumor_id, true, false) != -1)
		{
			int index_ro = variants.annotationIndexByName("RO", tumor_id);
			int index_ao = variants.annotationIndexByName("AO", tumor_id);
			int count_mut = variants[i].annotations()[index_ao].toInt();
			int count_all = count_mut + variants[i].annotations()[index_ro].toInt();
			if(count_all>0)
			{
				hist_all.inc((double)count_mut/count_all);
				if(variants[i].filters().empty())	hist_filtered.inc((double)count_mut/count_all);
			}
		}
		//mutect
		//##FORMAT=<ID=FA,Number=A,Type=Float,Description="Allele fraction of the alternate allele with regard to reference">
		else if(variants.annotationIndexByName("FA", tumor_id, true, false) != -1)
		{
			int index_fa = variants.annotationIndexByName("FA", tumor_id);
			hist_all.inc(variants[i].annotations()[index_fa].toDouble());;
			if(variants[i].filters().empty())	hist_filtered.inc(variants[i].annotations()[index_fa].toDouble());
		}
		// else: strelka indel
	}

	QString plot0name = Helper::tempFileName(".png");
	hist_all.setLabel("all variants");
	hist_filtered.setLabel("variants with filter PASS");
	Histogram::storeCombinedHistogram(plot0name, QList<Histogram>({hist_all,hist_filtered}),"tumor allele frequency","count");
	output.insert(QCValue::Image("somatic SNVs allele frequency histogram", plot0name, "Allele frequency histogram of somatic SNVs.", "QC:2000055"));
	QFile::remove(plot0name);

	//plot0b: absolute count mutation distribution
	BarPlot plot0b;
	plot0b.setXLabel("base change");
	plot0b.setYLabel("count");
	QMap<QString,QString> color_map = QMap<QString,QString>{{"C>A","b"},{"C>G","k"},{"C>T","r"},{"T>A","g"},{"T>G","c"},{"T>C","y"}};
	foreach(QString color, color_map)
	{
		plot0b.addColorLegend(color,color_map.key(color));
	}

	QList<int> counts({0,0,0,0,0,0});
	QList<QString> nuc_changes({"C>A","C>G","C>T","T>A","T>G","T>C"});
	QList<QString> colors({"b","k","r","g","c","y"});
	for(int i=0; i<variants.count(); ++i)
	{
		if(!variants[i].filters().empty())	continue;	//skip non-somatic variants
		if(!variants[i].isSNV())	continue;	//skip indels

		Variant v = variants[i];
		QString n = v.ref()+">"+v.obs();
		bool contained = false;
		if(nuc_changes.contains(n))	contained = true;
		else
		{
			n = NGSHelper::changeSeq(v.ref(),true,true) + ">" + NGSHelper::changeSeq(v.obs(),true,true);
			if(nuc_changes.contains(n))	contained = true;
		}

		if(!contained)
		{
			Log::warn("Unidentified nucleotide change " + n);
			continue;
		}
		++counts[nuc_changes.indexOf(n)];
	}

	int ymax = 0;
	foreach(int c, counts)
	{
		if(c>ymax)	ymax = c;
	}

	plot0b.setYRange(-ymax*0.02,ymax*1.2);
	plot0b.setXRange(-1.5,nuc_changes.count()+0.5);
	plot0b.setValues(counts, nuc_changes, colors);
	QString plot0bname = Helper::tempFileName(".png");
	plot0b.store(plot0bname);
	output.insert(QCValue::Image("somatic SNV mutation types", plot0bname, "", "QC:?"));
	QFile::remove(plot0bname);

	//plot1: allele frequencies
	ScatterPlot plot1;
	plot1.setXLabel("tumor allele frequency");
	plot1.setYLabel("normal allele frequency");
	plot1.setXRange(-0.015,1.015);
	plot1.setYRange(-0.015,1.015);
	QList< QPair<double,double> > points_black;
	QList< QPair<double,double> > points_green;
	for(int i=0; i<variants.count(); ++i)
	{
		double af_tumor = -1;
		double af_normal = -1;
		int count_mut = 0;
		int count_all = 0;

		//strelka SNV tumor and normal
		int idx_strelka_snv = variants.annotationIndexByName("AU", tumor_id, true, false);
		int idx_strelka_indel = variants.annotationIndexByName("TIR", tumor_id, true, false);
		if( idx_strelka_snv!=-1 && !variants[i].annotations()[idx_strelka_snv].isEmpty() )
		{			
			count_mut = 0;
			count_all = 0;
			foreach(QString n, nuc)
			{
				int index_n = variants.annotationIndexByName((n+"U"), tumor_id);
				int tmp = variants[i].annotations()[index_n].split(',')[0].toInt();
				if(n==variants[i].obs())	count_mut += tmp;
				count_all += tmp;
			}
			if(count_all>0)	af_tumor = (double)count_mut/count_all;

			count_mut = 0;
			count_all = 0;
			foreach(QString n, nuc)
			{
				int index_n = variants.annotationIndexByName((n+"U"), normal_id);
				int tmp = variants[i].annotations()[index_n].split(',')[0].toInt();
				if(n==variants[i].obs())	count_mut += tmp;
				count_all += tmp;
			}
			if(count_all>0)	af_normal = (double)count_mut/count_all;
		}
		else if( idx_strelka_indel!=-1 && !variants[i].annotations()[idx_strelka_indel].isEmpty() )	//indels strelka
		{
			//TIR + TAR tumor
			count_mut = 0;
			count_all = 0;
			int idx = variants.annotationIndexByName("TIR", tumor_id);
			count_mut = variants[i].annotations()[idx].split(',')[0].toInt();
			idx = variants.annotationIndexByName("TAR", tumor_id);
			count_all = variants[i].annotations()[idx].split(',')[0].toInt() + count_mut;
			if(count_all>0)	af_tumor = (double)count_mut/count_all;

			//TIR + TAR normal
			count_mut = 0;
			count_all = 0;
			idx = variants.annotationIndexByName("TIR", normal_id);
			count_mut = variants[i].annotations()[idx].split(',')[0].toInt();
			idx = variants.annotationIndexByName("TAR", normal_id);
			count_all = variants[i].annotations()[idx].split(',')[0].toInt() + count_mut;
			if(count_all>0)	af_normal = (double)count_mut/count_all;
		}
		//freebayes tumor and normal
		//##FORMAT=<ID=RO,Number=1,Type=Integer,Description="Reference allele observation count">
		//##FORMAT=<ID=AO,RONumber=A,Type=Integer,Description="Alternate allele observation count">
		else if(variants.annotationIndexByName("AO", tumor_id, true, false) != -1)
		{
			int index_ro = variants.annotationIndexByName("RO", tumor_id);
			int index_ao = variants.annotationIndexByName("AO", tumor_id);
			count_mut = variants[i].annotations()[index_ao].toInt();
			count_all = count_mut + variants[i].annotations()[index_ro].toInt();
			if(count_all>0)	af_tumor = (double)count_mut/count_all;

			index_ro = variants.annotationIndexByName("RO", normal_id);
			index_ao = variants.annotationIndexByName("AO", normal_id);
			count_mut = variants[i].annotations()[index_ao].toInt();
			count_all = count_mut + variants[i].annotations()[index_ro].toInt();
			if(count_all>0)	af_normal = (double)count_mut/count_all;
		}
		//mutect
		//##FORMAT=<ID=FA,Number=A,Type=Float,Description="Allele fraction of the alternate allele with regard to reference">
		else if(variants.annotationIndexByName("FA", tumor_id, true, false) != -1)
		{
			int index_fa = variants.annotationIndexByName("FA", tumor_id);
			af_tumor = variants[i].annotations()[index_fa].toDouble();

			index_fa = variants.annotationIndexByName("FA", normal_id);
			af_normal = variants[i].annotations()[index_fa].toDouble();
		}
		else
		{
			Log::error("Could not identify vcf format in line " + QString::number(i+1) + ". Sample-ID: " + tumor_id + ". Position " + variants[i].chr().str() + ":" + QString::number(variants[i].start()) + ". Only strelka and freebayes are currently supported.");
		}

		//find AF and set x and y points, implement freebayes and strelka fields
		QPair<double,double> point;
		point.first = af_tumor;
		point.second = af_normal;
		if (!variants[i].filters().empty())	points_black.append(point);
		else points_green.append(point);
	}

	QList< QPair<double,double> > points;
	points  << points_black << points_green;

	QString g = "k";
	QString b = "g";
	colors.clear();
	for(int i=0;i<points_black.count();++i)
	{
		colors.append(g);
	}
	for(int i=0;i<points_green.count();++i)
	{
		colors.append(b);
	}

	plot1.setValues(points, colors);
	plot1.addColorLegend(g,"all variants");
	plot1.addColorLegend(b,"variants with filter PASS");

	QString plot1name = Helper::tempFileName(".png");
	plot1.store(plot1name);
	output.insert(QCValue::Image("somatic variants allele frequencies plot", plot1name, ".", "QC:2000048"));
	QFile::remove(plot1name);

	//plot2: somatic variant signature
	BarPlot plot2;
	plot2.setXLabel("triplett");
	plot2.setYLabel("count");
	QString c,co,cod;
	color_map = QMap<QString,QString>{{"C>A","b"},{"C>G","k"},{"C>T","r"},{"T>A","g"},{"T>G","c"},{"T>C","y"}};
	foreach(QString color, color_map)
	{
		plot2.addColorLegend(color,color_map.key(color));
	}
	QList<QString> codons;
	counts.clear();
	QList<double> counts_normalized;
	QList<double> frequencies;
	QList<QString> labels;
	colors = QList<QString>();
	QStringList sig = QStringList{"C","T"};
	foreach(QString r, sig)
	{
		c = r;
		foreach(QString o, nuc)
		{
			if(c == o)	continue;
			foreach(QString rr, nuc)
			{
				co = rr + c;
				foreach(QString rrr, nuc)
				{
					cod = co + rrr + " - " + o;
					codons.append(cod);
					counts.append(0);
					counts_normalized.append(0);
					frequencies.append(0);
					colors.append(color_map[r+">"+o]);
					labels.append(co + rrr);
				}
			}
		}
	}
	//codons: count codons from variant list
	FastaFileIndex reference(ref_fasta);
	for(int i=0; i<variants.count(); ++i)
	{
		if(!variants[i].filters().empty())	continue;	//skip non-somatic variants
		if(!variants[i].isSNV())	continue;	//skip indels

		Variant v = variants[i];

		bool contained = false;
		QString c = reference.seq(v.chr(),v.start()-1,1,true) + v.ref().toUpper() + reference.seq(v.chr(),v.start()+1,1,true) + " - " + v.obs().toUpper();
		if(codons.contains(c))	contained = true;
		else
		{
			c = NGSHelper::changeSeq(reference.seq(v.chr(),v.start()-1,1,true).toUpper() + v.ref().toUpper() + reference.seq(v.chr(),v.start()+1,1,true).toUpper(),true,true) + " - " + NGSHelper::changeSeq(v.obs().toUpper(),true,true);
			if(codons.contains(c))	contained = true;
		}

		if(!contained)	continue;
		++counts[codons.indexOf(c)];
	}

//	for(int i=0; i<codons.length();++i)
//	{
//		qDebug() << codons[i] << QString::number(counts[i]);
//	}

	plot2.setYLabel("variant type percentage");
	if(target_file.count() != 0)	plot2.setYLabel("normalized variant type percentage");
	QHash < QString, int > count_codons_target({
	   {"ACA",0},{"ACC",0},{"ACG",0},{"ACT",0},{"CCA",0},{"CCC",0},{"CCG",0},{"CCT",0},
	   {"GCA",0},{"GCC",0},{"GCG",0},{"GCT",0},{"TCA",0},{"TCC",0},{"TCG",0},{"TCT",0},
	   {"ATA",0},{"ATC",0},{"ATG",0},{"ATT",0},{"CTA",0},{"CTC",0},{"CTG",0},{"CTT",0},
	   {"GTA",0},{"GTC",0},{"GTG",0},{"GTT",0},{"TTA",0},{"TTC",0},{"TTG",0},{"TTT",0}
	});
	if(target_file.count() == 0)
	{
		FastaFileIndex reference(ref_fasta);
		int bin = 50000000;
		for(int i=0; i<reference.names().count(); ++i)
		{
			Chromosome chr = reference.names().at(i);

			if(!chr.isNonSpecial()) continue;

			int chrom_length = reference.lengthOf(chr);
			for(int j=1; j<=chrom_length; j+=bin)
			{
				int start = j;
				int length = bin;
				if(start>1)	//make bins overlap
				{
					start -= 2;
					length += 2;
				}
				if((start+length-1)>chrom_length)	length = (chrom_length - start + 1);
				Sequence seq = reference.seq(chr,start,length,true);
				foreach(QString codon, count_codons_target.keys())
				{
					count_codons_target[codon] += seq.count(codon.toUpper().toLatin1());
					count_codons_target[codon] += seq.count(NGSHelper::changeSeq(codon.toLatin1(),true,true));
				}
			}
		}
	}
	else
	{
		//codons: count in target file
		if(target_file.baseCount()<100000)	Log::warn("Target size is less than 100 kb. Mutation signature may be imprecise.");
		for(int i=0; i<target_file.count(); ++i)
		{
			Sequence seq = reference.seq(target_file[i].chr(),target_file[i].start(),target_file[i].length(),true);
			foreach(QString codon, count_codons_target.keys())
			{
				count_codons_target[codon] += seq.count(codon.toLatin1());
				count_codons_target[codon] += seq.count(NGSHelper::changeSeq(codon.toLatin1(),true,true));
			}
		}
	}

//	foreach(QString codon, count_codons_target.keys())
//	{
//		qDebug() << codon << QString::number(count_codons_target[codon]);
//	}

	//codons: normalize current codons and calculate percentages for each codon
	double y_max = 5;
	double sum = 0;
	for(int i=0; i<codons.count(); ++i)
	{
		QString cod = codons[i].mid(0,3);
		counts_normalized[i] = counts[i]/double(count_codons_target[cod]);
		sum += counts_normalized[i];
	}
	if(sum!=0)
	{
		for(int i=0; i<counts_normalized.count(); ++i)
		{
			frequencies[i] = counts_normalized[i]/sum * 100;
			if(frequencies[i]>y_max)	y_max = frequencies[i];
		}
	}

	plot2.setXRange(-1.5,frequencies.count()+0.5);
	plot2.setYRange(-y_max*0.02,y_max*1.2);
	plot2.setValues(frequencies, labels, colors);
	QString plot2name = Helper::tempFileName(".png");
	plot2.store(plot2name);
	output.insert(QCValue::Image("somatic SNV signature plot", plot2name, "Percentage of different variant types. If a target file was given, the variant type percentage is normalized to the reference genome.", "QC:2000047"));
	QFile::remove(plot2name);

	//plot3: somatic variant distances, only for whole genome sequencing
	if(target_file.count() == 0)
	{
		ScatterPlot plot3;
		plot3.setXLabel("chromosomes");
		plot3.setYLabel("somatic variant distance [bp]");
		plot3.setYLogScale(true);
		//(0) generate chromosomal map
		long long genome_size = 0;
		QMap<Chromosome,long long> chrom_starts;
		QStringList fai = Helper::loadTextFile(ref_fasta + ".fai", true, '~', true);
		foreach(QString line, fai)
		{
			QStringList parts = line.split("\t");
			if (parts.count()<2) continue;
			bool ok = false;
			int value = parts[1].toInt(&ok);
			if (!ok) continue;
			Chromosome c = Chromosome(parts[0]);
			if(!c.isNonSpecial())	continue;
			chrom_starts[c] = genome_size;
			genome_size += value;
		}
		QMap<Chromosome,double> chrom_starts_norm;
		foreach(Chromosome c, chrom_starts.keys())
		{
			double offset = double(chrom_starts[c])/double(genome_size);
			chrom_starts_norm[c] = offset;
		}
		//(1) add chromosome lines
		foreach(Chromosome c, chrom_starts.keys())
		{
			if(chrom_starts_norm[c]==0)	continue;
			plot3.addVLine(chrom_starts_norm[c]);
		}
		//(2) calculate distance for each chromosome and convert it to x-coordinates
		QList< QPair<double,double> > points3;
		QString tmp_chr = "";
		int tmp_pos = 0;
		double tmp_offset = 0;
		double max = 0;
		for(int i=0; i<variants.count(); ++i)	//list has to be sorted by chrom. position
		{
			if(!variants[i].chr().isNonSpecial())	continue;
			if(!variants[i].filters().empty())	continue;	//skip non-somatic variants

			if(tmp_chr == variants[i].chr().str())	//same chromosome
			{
				//convert distance to x-Axis position
				QPair<double,double> point;
				point.first = tmp_offset + double(variants[i].start())/double(genome_size);
				point.second = variants[i].start() - tmp_pos;
				if(max < point.second)	max = point.second;
				points3.append(point);
			}

			if((tmp_chr != variants[i].chr().str()) && i>0)	//if a different chromosome is found => udpate offset
			{
				if(!chrom_starts_norm.contains(variants[i].chr()))	continue; //skip invalid chromosomes
				tmp_pos = 0;
				tmp_offset = chrom_starts_norm[tmp_chr];
			}

			tmp_chr = variants[i].chr().str();
			tmp_pos = variants[i].start();
		}

		plot3.setYRange(0.975,max*100);
		plot3.setXRange(0,1);
		plot3.noXTicks();
		plot3.setValues(points3);
		QString plot3name = Helper::tempFileName(".png");
		plot3.store(plot3name);
		output.insert(QCValue::Image("somatic variant distance plot", plot3name, ".", "QC:2000046"));
		QFile::remove(plot3name);
	}

	return output;
}

QCCollection Statistics::contamination(QString build, QString bam, bool debug, int min_cov, int min_snps)
{
	//open BAM
	BamReader reader(bam);

	//calcualate frequency histogram
	Histogram hist(0, 1, 0.05);
	int passed = 0;
	double passed_depth_sum = 0.0;
	VariantList snps = NGSHelper::getKnownVariants(build, true, 0.2, 0.8);
	for(int i=0; i<snps.count(); ++i)
	{
		Pileup pileup = reader.getPileup(snps[i].chr(), snps[i].start());
		int depth = pileup.depth(false);
		if (depth<min_cov) continue;

		double freq = pileup.frequency(snps[i].ref()[0], snps[i].obs()[0]);

		//skip non-informative snps
		if (!BasicStatistics::isValidFloat(freq)) continue;

		++passed;
		passed_depth_sum += depth;

		hist.inc(freq);
	}

	//debug output
	if (debug)
	{
		QTextStream stream(stdout);
		stream << "Contamination debug output:\n";
		stream << passed << " of " << snps.count() << " SNPs passed quality filters\n";
		stream << "Average depth of passed SNPs: " << QString::number(passed_depth_sum/passed,'f', 2) << "\n";
		stream << "\nAF histogram:\n";
		hist.print(stream, "", 2, 0);
	}

	//output
	double off = 0.0;
	for (int i=1; i<=5; ++i) off += hist.binValue(i, true);
	for (int i=14; i<=18; ++i) off += hist.binValue(i, true);
	QCCollection output;
	QString value = (passed < min_snps) ? "n/a" : QString::number(off, 'f', 2);
	output.insert(QCValue("SNV allele frequency deviation", value, "Percentage of common SNPs that deviate from the expected allele frequency (i.e. 0.0, 0.5 or 1.0 for diploid organisms)", "QC:2000051"));
	return output;
}

AncestryEstimates Statistics::ancestry(QString build, const VariantList& vl, int min_snp, double min_pop_dist)
{
	//determine required annotation indices
	int i_gt = vl.annotationIndexByName("GT");

	//load ancestry-informative SNP list
	QString snp_file = ":/Resources/" + build + "_ancestry.vcf";
	if (!QFile::exists(snp_file)) THROW(ProgrammingException, "Unsupported genome build '" + build + "'!");
	VariantList af;
	af.load(snp_file, VCF);
	ChromosomalIndex<VariantList> af_idx(af);
	int i2_afr = af.annotationIndexByName("AF_AFR");
	int i2_eur = af.annotationIndexByName("AF_EUR");
	int i2_sas = af.annotationIndexByName("AF_SAS");
	int i2_eas = af.annotationIndexByName("AF_EAS");

	//process variants
	QVector<double> geno_sample;
	QVector<double> af_afr;
	QVector<double> af_eur;
	QVector<double> af_sas;
	QVector<double> af_eas;
	for(int i=0; i<vl.count(); ++i)
	{
		const Variant& v = vl[i];

		//skip non-informative SNPs
		int index = af_idx.matchingIndex(v.chr(), v.start(), v.end());
		if (index==-1) continue;
		const Variant& v2 = af[index];
		if (v.ref()!=v2.ref() || v.obs()!=v2.obs()) continue;

		//genotype sample
		geno_sample << vl[i].annotations().at(i_gt).count('1');

		//population AFs
		af_afr << v2.annotations().at(i2_afr).toDouble();
		af_eur << v2.annotations().at(i2_eur).toDouble();
		af_sas << v2.annotations().at(i2_sas).toDouble();
		af_eas << v2.annotations().at(i2_eas).toDouble();
	}

	//not enough informative SNPs
	AncestryEstimates output;
	output.snps = geno_sample.count();
	if (geno_sample.count()<min_snp)
	{
		output.afr = std::numeric_limits<double>::quiet_NaN();
		output.eur = std::numeric_limits<double>::quiet_NaN();
		output.sas = std::numeric_limits<double>::quiet_NaN();
		output.eas = std::numeric_limits<double>::quiet_NaN();
		output.population = "NOT_ENOUGH_SNPS";
		return output;
	}

	//compose output
	output.afr = BasicStatistics::correlation(geno_sample, af_afr);
	if (output.afr<0) output.afr = 0.0;
	output.eur = BasicStatistics::correlation(geno_sample, af_eur);
	if (output.eur<0) output.eur = 0.0;
	output.sas = BasicStatistics::correlation(geno_sample, af_sas);
	if (output.sas<0) output.sas = 0.0;
	output.eas = BasicStatistics::correlation(geno_sample, af_eas);
	if (output.eas<0) output.eas = 0.0;

	//determine population
	QList<QPair<QString, double>> sorted;
	sorted << QPair<QString, double>("AFR", output.afr);
	sorted << QPair<QString, double>("EUR", output.eur);
	sorted << QPair<QString, double>("SAS", output.sas);
	sorted << QPair<QString, double>("EAS", output.eas);
	std::sort(sorted.begin(), sorted.end(), [](const QPair<QString, double>& a, const QPair<QString, double>& b){ return a.second > b.second ;});
	output.population = sorted[0].first;
	if (sorted[1].second/sorted[0].second>1.0-min_pop_dist)
	{
		output.population = "ADMIXED/UNKNOWN";
	}

	return output;
}

BedFile Statistics::lowCoverage(const BedFile& bed_file, const QString& bam_file, int cutoff, int min_mapq)
{
	BedFile output;

    //check target region is merged/sorted and create index
    if (!bed_file.isMergedAndSorted())
    {
		THROW(ArgumentException, "Merged and sorted BED file required for low-coverage statistics!");
    }

    //open BAM file
	BamReader reader(bam_file);

	//iterate trough all regions (i.e. exons in most cases)
	for (int i=0; i<bed_file.count(); ++i)
	{
		const BedLine& bed_line = bed_file[i];
		const int start = bed_line.start();
		//qDebug() << bed_line.chr().str().constData() << ":" << bed_line.start() << "-" << bed_line.end();

		//init coverage statistics
        QVector<int> roi_cov(bed_line.length(), 0);

		//jump to region
		reader.setRegion(bed_line.chr(), bed_line.start(), bed_line.end());

		//iterate through all alignments
		BamAlignment al;
		while (reader.getNextAlignment(al))
		{
			if (al.isDuplicate()) continue;
			if (al.isSecondaryAlignment()) continue;
			if (al.isUnmapped() || al.mappingQuality()<min_mapq) continue;

			const int ol_start = std::max(start, al.start()) - start;
			const int ol_end = std::min(bed_line.end(), al.end()) - start;
			for (int p=ol_start; p<=ol_end; ++p)
			{
				++roi_cov[p];
			}
		}

        //create low-coverage regions file
		bool reg_open = false;
		int reg_start = -1;
        for (int p=0; p<roi_cov.count(); ++p)
        {
            bool low_cov = roi_cov[p]<cutoff;
            if (reg_open && !low_cov)
            {
				output.append(BedLine(bed_line.chr(), reg_start+start, p+start-1, bed_line.annotations()));
                reg_open = false;
                reg_start = -1;
            }
            if (!reg_open && low_cov)
            {
                reg_open = true;
                reg_start = p;
            }
        }
        if (reg_open)
        {
			output.append(BedLine(bed_line.chr(), reg_start+start, bed_line.length()+start-1, bed_line.annotations()));
        }
    }

	return output;
}

BedFile Statistics::lowCoverage(const QString& bam_file, int cutoff, int min_mapq)
{
	if (cutoff>255) THROW(ArgumentException, "Cutoff cannot be bigger than 255!");

    BedFile output;

    //open BAM file
	BamReader reader(bam_file);

	QVector<unsigned char> cov;

	//iteratore through chromosomes
	foreach(const Chromosome& chr, reader.chromosomes())
    {
        if (!chr.isNonSpecial()) continue;

		int chr_size = reader.chromosomeSize(chr);
		cov.fill(0, chr_size);

		//jump to chromosome
		reader.setRegion(chr, 0, chr_size);

		//iterate through all alignments
        BamAlignment al;
		while (reader.getNextAlignment(al))
        {
			if (al.isDuplicate()) continue;
			if (al.isSecondaryAlignment()) continue;
			if (al.isUnmapped() || al.mappingQuality()<min_mapq) continue;

			const int end = al.end();
			for (int p=al.start()-1; p<end; ++p)
            {
				if (cov[p]<254) ++cov[p];
            }
        }

		//create low-coverage regions file
		bool reg_open = false;
		int reg_start = -1;
		for (int p=0; p<chr_size; ++p)
		{
			bool low_cov = cov[p]<cutoff;
			if (reg_open && !low_cov)
			{
				output.append(BedLine(chr, reg_start+1, p));
				reg_open = false;
				reg_start = -1;
			}
			if (!reg_open && low_cov)
			{
				reg_open = true;
				reg_start = p;
			}
		}
		if (reg_open)
		{
			output.append(BedLine(chr, reg_start+1, chr_size));
		}
	}

	output.merge();
    return output;
}

void Statistics::avgCoverage(BedFile& bed_file, const QString& bam_file, int min_mapq, bool include_duplicates, bool panel_mode, int decimals)
{
    //check target region is merged/sorted and create index
    if (!bed_file.isMergedAndSorted())
    {
        THROW(ArgumentException, "Merged and sorted BED file required for coverage calculation!");
    }

    //open BAM file
	BamReader reader(bam_file);

	if (panel_mode) //panel mode
	{
		for (int i=0; i<bed_file.count(); ++i)
		{
			long cov = 0;
			BedLine& bed_line = bed_file[i];

			//jump to region
			reader.setRegion(bed_line.chr(), bed_line.start(), bed_line.end());

			//iterate through all alignments
			BamAlignment al;
			while (reader.getNextAlignment(al))
			{
				if (!include_duplicates && al.isDuplicate()) continue;
				if (al.isSecondaryAlignment()) continue;
				if (al.isUnmapped() || al.mappingQuality()<min_mapq) continue;

				const int ol_start = std::max(bed_line.start(), al.start());
				const int ol_end = std::min(bed_line.end(), al.end());
				if (ol_start<=ol_end)
				{
					cov += ol_end - ol_start + 1;
				}
			}
			bed_line.annotations().append(QByteArray::number((double)cov / bed_line.length(), 'f', decimals));
		}
	}
	else //default mode
	{
		//init coverage statistics data structure
		QVector<long> cov;
		cov.fill(0, bed_file.count());

		//iterate through all alignments
		ChromosomalIndex<BedFile> bed_idx(bed_file);
		BamAlignment al;
		while (reader.getNextAlignment(al))
		{
			if (!include_duplicates && al.isDuplicate()) continue;
			if (al.isSecondaryAlignment()) continue;
			if (al.isUnmapped() || al.mappingQuality()<min_mapq) continue;

			const Chromosome& chr = reader.chromosome(al.chromosomeID());
			int end_position = al.end();
			QVector<int> indices = bed_idx.matchingIndices(chr, al.start(), end_position);
			foreach(int index, indices)
			{
				cov[index] += std::min(bed_file[index].end(), end_position) - std::max(bed_file[index].start(), al.start());
			}
		}

		//calculate output
		for (int i=0; i<bed_file.count(); ++i)
		{
			bed_file[i].annotations().append(QByteArray ::number((double)(cov[i]) / bed_file[i].length(), 'f', decimals));
		}
	}
}

BedFile Statistics::highCoverage(const QString& bam_file, int cutoff, int min_mapq)
{
	if (cutoff>255) THROW(ArgumentException, "Cutoff cannot be bigger than 255!");

	BedFile output;

	//open BAM file
	BamReader reader(bam_file);

	QVector<unsigned char> cov;

	//iteratore through chromosomes
	foreach(const Chromosome& chr, reader.chromosomes())
	{
		if (!chr.isNonSpecial()) continue;

		const int chr_size = reader.chromosomeSize(chr);
		//qDebug() << Helper::dateTime() << "starting" << chr.str() << chr_size << cov.capacity() << output.count();
		cov.fill(0, chr_size);

		//jump to chromosome
		reader.setRegion(chr, 0, chr_size);

		//iterate through all alignments
		int seg_start = 0;
		int seg_end = -1;
		bool seg_has_high_af = false;
		BamAlignment al;
		while (reader.getNextAlignment(al))
		{
			//skip unusable data
			if (al.isDuplicate()) continue;
			if (al.isSecondaryAlignment()) continue;
			if (al.isUnmapped() || al.mappingQuality()<min_mapq) continue;

			//increase counts
			const int end = al.end();
			for (int p=al.start()-1; p<end; ++p)
			{
				if (cov[p]<254) ++cov[p];

				if (!seg_has_high_af && cov[p]>=cutoff) seg_has_high_af = true;
			}

			//gap between segments => check last coverated segment for high-coverage data
			if (seg_end < al.start())
			{
				//qDebug() << "SEG" << seg_start << seg_end << seg_has_high_af;
				if (seg_has_high_af)
				{
					for (int p=seg_start-1; p<seg_end; ++p)
					{
						if (cov[p]>=cutoff)
						{
							const int start = p + 1;
							while(p<seg_end && cov[p]>=cutoff)
							{
								++p;
							}
							//qDebug() << "  LINE" << chr.str() << start << p;
							output.append(BedLine(chr, start, p));
						}
					}
				}

				seg_start = al.start();
				seg_has_high_af = false;
			}

			//update last end
			seg_end = std::max(seg_end, end);
		}

		//qDebug() << "SEG" << seg_start << seg_end << seg_has_high_af;
		if (seg_has_high_af)
		{
			for (int p=seg_start-1; p<seg_end; ++p)
			{
				if (cov[p]>=cutoff)
				{
					const int start = p + 1;
					while(p<seg_end && cov[p]>=cutoff)
					{
						++p;
					}
					//qDebug() << "  LINE" << chr.str() << start << p;
					output.append(BedLine(chr, start, p));
				}
			}
		}

	}

	output.merge();
	return output;
}

GenderEstimate Statistics::genderXY(QString bam_file, double max_female, double min_male)
{
    //open BAM file
	BamReader reader(bam_file);

	//get RefID of X and Y chromosome

	//count reads on chrX
	int count_x = 0;
	Chromosome chrx("chrX");
	reader.setRegion(chrx, 1, reader.chromosomeSize(chrx));
	BamAlignment al;
	while (reader.getNextAlignment(al))
	{
		++count_x;
	}

	//count reads on chrY
	int count_y = 0;
	Chromosome chry("chrY");
	reader.setRegion(chry, 1, reader.chromosomeSize(chry));
	while (reader.getNextAlignment(al))
	{
		++count_y;
	}
	double ratio_yx = (double) count_y / count_x;

	//output
	GenderEstimate output;
	output.add_info << KeyValuePair("reads_chry", QString::number(count_y));
	output.add_info << KeyValuePair("reads_chrx", QString::number(count_x));
	output.add_info << KeyValuePair("ratio_chry_chrx", QString::number(ratio_yx, 'f', 4));

    //output
	if (ratio_yx<=max_female) output.gender = "female";
	else if (ratio_yx>=min_male) output.gender = "male";
	else output.gender = "unknown (ratio in gray area)";

	return output;
}

GenderEstimate Statistics::genderHetX(QString bam_file, QString build, double max_male, double min_female)
{
    //open BAM file
	BamReader reader(bam_file);

    //restrict to X chromosome
	Chromosome chrx("chrX");
	int chrx_end_pos = reader.chromosomeSize(chrx);
	reader.setRegion(chrx, 1, chrx_end_pos);

	//load SNPs on chrX
	BedFile roi_chrx("chrX", 1, chrx_end_pos);
	VariantList snps = NGSHelper::getKnownVariants(build, true, 0.2, 0.8, &roi_chrx);
    QVector<Pileup> counts;
	counts.fill(Pileup(), snps.count());

    //iterate through all alignments and create counts
    BamAlignment al;
	while (reader.getNextAlignment(al))
    {
		if (al.mappingQuality()<20) continue;

		int start = al.start();
		int end = al.end();

		for (int i=0; i<snps.count(); ++i)
        {
			int pos = snps[i].start();
            if (start <= pos && end >= pos)
			{
				QPair<char, int> base = al.extractBaseByCIGAR(pos);
                counts[i].inc(base.first);
            }
        }
	}

    //count
	int hom_count = 0;
    int het_count = 0;
	for (int i=0; i<snps.count(); ++i)
    {
        int depth = counts[i].depth(false);
        if (depth>=30)
        {
            int max = counts[i].max();
            if (max<0.8 * depth)
            {
                ++het_count;
            }
            else
            {
                ++hom_count;
            }
        }
    }
	double het_frac = (double) het_count / (het_count + hom_count);

	//output
	GenderEstimate output;
	output.add_info << KeyValuePair("snps_usable", QString::number(hom_count + het_count) + " of " + QString::number(snps.count()));
	output.add_info << KeyValuePair("hom_count", QString::number(hom_count));
	output.add_info << KeyValuePair("het_count", QString::number(het_count));
	output.add_info << KeyValuePair("het_fraction", QString::number(het_frac, 'f', 4));

	if (hom_count + het_count < 20) output.gender = "unknown (too few SNPs)";
	else if (het_frac<=max_male) output.gender = "male";
	else if (het_frac>=min_female) output.gender = "female";
	else output.gender = "unknown (fraction in gray area)";

	return output;
}

GenderEstimate Statistics::genderSRY(QString bam_file, QString build, double min_cov)
{
	//open BAM file
	BamReader reader(bam_file);

	//restrict to SRY gene
	int start = 2655031;
	int end = 2655641;
	if (build=="hg38")
	{
		start = 2786989;
		end = 2787603;
	}
	else if (build!="hg19")
	{
		THROW(ProgrammingException, "Unsupported genome build '" + build + "'!");
	}
	reader.setRegion(Chromosome("chrY"), start, end);

	//calcualte average coverage
	double cov = 0.0;
	BamAlignment al;
	while (reader.getNextAlignment(al))
	{
		cov += al.length();
	}
	cov /= (end-start);

	//output
	GenderEstimate output;
	output.add_info << KeyValuePair("coverage_sry", QString::number(cov, 'f', 2));
	output.gender = cov>=min_cov ? "male" : "female";
	return output;
}
