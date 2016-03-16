#include "TestFramework.h"
#include "QFileInfo"
#include <stdlib.h>

TEST_CLASS(BamDeduplicateByBarcode_Test)
{
Q_OBJECT
private slots:

	void test_bam_without_duplicates()
	{
		EXECUTE("BamDeduplicateByBarcode", "-bam " + TESTDATA("data_in/BamDeduplicateByBarcode_in.bam") + " -index " + TESTDATA("data_in/BamDeduplicateByBarcode_in.fastq.gz") + " -out out/BamDeduplicateByBarcode_out.bam");
		IS_TRUE(QFile::exists("out/BamDeduplicateByBarcode_out.bam"));
	}

	void test_bam_with_many_duplicates_same_barcode()
	{
		EXECUTE("BamDeduplicateByBarcode", "-bam " + TESTDATA("data_in/BamDeduplicateByBarcode_in2.bam") + " -index " + TESTDATA("data_in/BamDeduplicateByBarcode_in2.fastq") + " -out out/BamDeduplicateByBarcode_out2.bam");
		IS_TRUE(QFile::exists("out/BamDeduplicateByBarcode_out2.bam"));
	}

	void test_bam_with_many_duplicates_and_unique_barcodes()
	{
		EXECUTE("BamDeduplicateByBarcode", "-bam " + TESTDATA("data_in/BamDeduplicateByBarcode_in2.bam") + " -index " + TESTDATA("data_in/BamDeduplicateByBarcode_in3.fastq") + " -out out/BamDeduplicateByBarcode_out3.bam");
		IS_TRUE(QFile::exists("out/BamDeduplicateByBarcode_out2.bam"));
	}

	void test_compare_deduplicated_with_same_barcode_to_unique_barcodes()
	{
		QFileInfo same(TESTDATA("data_in/BamDeduplicateByBarcode_in2.bam"));
		QFileInfo unique("out/BamDeduplicateByBarcode_out3.bam");
		IS_TRUE((unique.size()-same.size())>8000);
	}

	void test_duplicates_regarding_mip_file()
	{
		EXECUTE("BamDeduplicateByBarcode", "-bam " + TESTDATA("data_in/BamDeduplicateByBarcode_in4.bam") + " -index " + TESTDATA("data_in/BamDeduplicateByBarcode_index_in4.fastq.gz") + " -mip_file " + TESTDATA("data_in/FastqExtractBarcode_in_mips1.txt") +" -out out/BamDeduplicateByBarcode_out4.bam -mip_count_out out/BamDeduplicateByBarcode_out1.tsv");
		COMPARE_FILES("out/BamDeduplicateByBarcode_out1.tsv", TESTDATA("data_out/BamDeduplicateByBarcode_out1.tsv"));
	}

};
