#ifndef VCFBREAKCOMPLEXVARIANTS_TEST_H
#define VCFBREAKCOMPLEXVARIANTS_TEST_H
#include "TestFramework.h"
#include "TestFrameworkNGS.h"
#include "Settings.h"

TEST_CLASS(VcfBreakComplexVariants_TEST)
{
Q_OBJECT
private slots:
	void test_vt_cases()
	{
		// Test cases from VT, see https://github.com/atks/vt/tree/master/test/decompose_blocksub
		EXECUTE("VcfBreakComplexVariants", "-in " + TESTDATA("data_in/VcfBreakComplexVariants_in01.vcf") + " -out out/VcfBreakComplexVariants_out01.vcf");
		COMPARE_FILES("out/VcfBreakComplexVariants_out01.vcf", TESTDATA("data_out/VcfBreakComplexVariants_out01.vcf"));
		VCF_IS_VALID("out/VcfBreakComplexVariants_out01.vcf");
	}

	void test_simple_snp_korell()
	{
		EXECUTE("VcfBreakComplexVariants", "-in " + TESTDATA("data_in/VcfBreakComplexVariants_in02.vcf") + " -out out/VcfBreakComplexVariants_out02.vcf");
		COMPARE_FILES("out/VcfBreakComplexVariants_out02.vcf", TESTDATA("data_out/VcfBreakComplexVariants_out02.vcf"));
		VCF_IS_VALID("out/VcfBreakComplexVariants_out02.vcf");
	}

	void test_no_tag()
	{
		EXECUTE("VcfBreakComplexVariants", "-in " + TESTDATA("data_in/VcfBreakComplexVariants_in02.vcf") + " -out out/VcfBreakComplexVariants_out03.vcf -no_tag");
		COMPARE_FILES("out/VcfBreakComplexVariants_out03.vcf", TESTDATA("data_out/VcfBreakComplexVariants_out03.vcf"));
		VCF_IS_VALID("out/VcfBreakComplexVariants_out03.vcf");
	}

	void test_keep_mnps()
	{
		EXECUTE("VcfBreakComplexVariants", "-in " + TESTDATA("data_in/VcfBreakComplexVariants_in04.vcf") + " -out out/VcfBreakComplexVariants_out04.vcf -keep_mnps");
		COMPARE_FILES("out/VcfBreakComplexVariants_out04.vcf", TESTDATA("data_out/VcfBreakComplexVariants_out04.vcf"));
		VCF_IS_VALID("out/VcfBreakComplexVariants_out04.vcf");
	}
};


#endif // VCFBREAKCOMPLEXVARIANTS_TEST_H
