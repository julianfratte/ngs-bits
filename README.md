# *ngs-bits* - Short-read sequencing tools

[![TravisCI status](https://api.travis-ci.org/imgag/ngs-bits.svg?branch=master)](https://travis-ci.org/imgag/ngs-bits/builds)
[![install with bioconda](https://img.shields.io/badge/install%20with-bioconda-brightgreen.svg?style=flat-square)](http://bioconda.github.io/recipes/ngs-bits/README.html)

## Obtaining ngs-bits

Binaries of *ngs-bits* are available via Bioconda. Alternatively, *ngs-bits* can be built from sources:

* **Binaries** for [Linux/macOS](doc/install_bioconda.md)
* From **sources** for [Linux/macOS](doc/install_unix.md)
* From **sources** for [Windows](doc/install_win.md)


## ChangeLog

Release 2018_04:

* Changed library used for BAM access to htslib (was bamtools).

## Support

Please report any issues or questions to the [ngs-bits issue 
tracker](https://github.com/imgag/ngs-bits/issues).

## Tools list

_ngs-bits_ contains a lot of tools that are used for NGS-based diagnostics in our [institute](http://www.uni-tuebingen.de/Klinische_Genetik/start.html):

### Main tools

* [SeqPurge](doc/tools/SeqPurge/index.md) - A highly-sensitive adapter trimmer for paired-end short-read data.
* [SampleCorrelation](doc/tools/SampleCorrelation/index.md) - Calculates the variant overlap and correlation of two VCF/BAM files.
* [SampleGender](doc/tools/SampleGender.md) - Determines sample gender based on a BAM file.
* [PERsim](doc/tools/PERsim.md) - Paired-end read simulator for Illumina reads.
* [CnvHunter](doc/tools/CnvHunter/index.md) - CNV detection from targeted resequencing data using non-matched control samples.
* [RohHunter](doc/tools/RohHunter.md) - ROH detection based on a variant list annotated with AF values.

### QC tools

The default output format of the quality control tools is [qcML](https://github.com/HUPO-PSI/qcML-development/), an XML-based format for -omics quality control, that consists of an [XML schema](https://github.com/HUPO-PSI/qcML-development/blob/master/schema/v0_0_8/qcML_0.0.8.xsd), which defined the overall structure of the format, and an [ontology](https://github.com/HUPO-PSI/qcML-development/blob/master/cv/qc-cv.obo) which defines the QC metrics that can be used.

* [ReadQC](doc/tools/ReadQC.md) - Quality control tool for FASTQ files.
* [MappingQC](doc/tools/MappingQC.md) - Quality control tool for a BAM file.
* [VariantQC](doc/tools/VariantQC.md) - Quality control tool for a VCF file.
* [SomaticQC](doc/tools/SomaticQC.md) - Quality control tool for tumor-normal pairs ([paper](https://www.ncbi.nlm.nih.gov/pubmed/28130233) and [example output data](doc/data/somatic_qc.zip?raw=true)).

### BAM tools

* [BamClipOverlap](doc/tools/BamClipOverlap.md) - (Soft-)Clips paired-end reads that overlap.
* [BamDownsample](doc/tools/BamDownsample.md) - Downsamples a BAM file to the given percentage of reads.
* [BamHighCoverage](doc/tools/BamHighCoverage.md) - Determines high-coverage regions in a BAM file.
* [BamToFastq](doc/tools/BamToFastq.md) - Converts a BAM file to FASTQ files (paired-end only).

### BED tools

* [BedAdd](doc/tools/BedAdd.md) - Merges regions from several BED files.
* [BedAnnotateFromBed](doc/tools/BedAnnotateFromBed.md) - Annotates BED file regions with information from a second BED file.
* [BedAnnotateGC](doc/tools/BedAnnotateGC.md) - Annnotates the regions in a BED file with GC content.
* [BedChunk](doc/tools/BedChunk.md) - Splits regions in a BED file to chunks of a desired size.
* [BedCoverage](doc/tools/BedCoverage.md) - Annotates the regions in a BED file with the average coverage in one or several BAM files.
* [BedExtend](doc/tools/BedExtend.md) - Extends the regions in a BED file by _n_ bases.
* [BedInfo](doc/tools/BedInfo.md) - Prints summary information about a BED file.
* [BedIntersect](doc/tools/BedIntersect.md) - Intersects two BED files.
* [BedLowCoverage](doc/tools/BedLowCoverage.md) - Calcualtes regions of low coverage based on a input BED and BAM file.
* [BedMerge](doc/tools/BedMerge.md) - Merges overlapping regions in a BED file.
* [BedReadCount](doc/tools/BedReadCount.md) - Annoates the regions in a BED file with the read count from a BAM file.
* [BedShrink](doc/tools/BedShrink.md) - Shrinks the regions in a BED file by _n_ bases.
* [BedSort](doc/tools/BedSort.md) - Sorts the regions in a BED file
* [BedSubtract](doc/tools/BedSubtract.md) - Subracts one BED file from another BED file.
* [BedToFasta](doc/tools/BedToFasta.md) - Converts BED file to a FASTA file (based on the reference genome).

### FASTQ tools

* [FastqConvert](doc/tools/FastqConvert.md) - Converts the quality scores from Illumina 1.5 offset to Sanger/Illumina 1.8 offset. 
* [FastqExtract](doc/tools/FastqExtract.md) - Extracts reads from a FASTQ file according to an ID list.
* [FastqExtractBarcode](doc/tools/FastqExtractBarcode.md) - Moves molecular barcodes of reads to a separate file.
* [FastqFormat](doc/tools/FastqFormat.md) - Determines the quality score offset of a FASTQ file.
* [FastqList](doc/tools/FastqList.md) - Lists read IDs and base counts.
* [FastqMidParser](doc/tools/FastqMidParser.md) - Counts the number of occurances of each MID/index/barcode in a FASTQ file.
* [FastqToFasta](doc/tools/FastqToFasta.md) - Converts FASTQ to FASTA format.
* [FastqTrim](doc/tools/FastqTrim.md) - Trims start/end bases from the reads in a FASTQ file.

### VCF tools

* [VcfAnnotateFromBed](doc/tools/VcfAnnotateFromBed.md) - Annotates the INFO column of a VCF with data from a BED file.
* [VariantFilterRegions](doc/tools/VariantFilterRegions.md) - Filter a variant list based on a target region.
* [VcfLeftNormalize](doc/tools/VcfLeftNormalize.md) - Normalizes all variants and shifts indels to the left in a VCF file.
* [VcfSort](doc/tools/VcfSort.md) - Sorts variant lists according to chromosomal position.
* [VcfStreamSort](doc/tools/VcfStreamSort.md) - Sorts entries of a VCF file according to genomic position using a stream.

### NGSD-based tools

Some of the tools need the NGSD, a MySQL database that contains for example gene, transcript and exon data.  
Installation instructions for the NGSD can be found [here](doc/install_ngsd.md).

* [BedAnnotateGenes](doc/tools/BedAnnotateGenes.md) - Annotates BED file regions with gene names.
* [BedGeneOverlap](doc/tools/BedGeneOverlap.md) - Calculates how much of each overlapping gene is covered.
* [GenesToApproved](doc/tools/GenesToApproved.md) - Replaces gene symbols by approved symbols using the HGNC database.
* [GenesToBed](doc/tools/GenesToBed.md) - Converts a text file with gene names to a BED file.

### Other tools

* [FastaInfo](doc/tools/FastaInfo.md) - Basic info on a FASTA file.


