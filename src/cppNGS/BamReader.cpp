#include "BamReader.h"
#include "Exceptions.h"
#include "NGSHelper.h"

#include <QFile>
#include <QFileInfo>

/*
External documentation used for the implementation:
- reading BAM file: https://gist.github.com/PoisonAlien/350677acc03b2fbf98aa
- jumping to regions: https://www.biostars.org/p/151053/
- extract qualities: https://github.com/broadinstitute/gamgee/blob/master/gamgee/sam/base_quals.cpp
- replacing CIGAR data: https://github.com/iontorrent/samtools/blob/master/padding.c
- replacing base/quality data with different length: https://github.com/samtools/htslib/issues/672
*/


BamAlignment::BamAlignment()
{
	aln_ = bam_init1();
}

BamAlignment::~BamAlignment()
{
	bam_destroy1(aln_);
}

BamAlignment::BamAlignment(const BamAlignment& rhs)
	: aln_(bam_dup1(rhs.aln_))
{
}

QList<CigarOp> BamAlignment::cigarData() const
{
	QList<CigarOp> output;

	const auto cigar = bam_get_cigar(aln_);
	for (uint32_t i = 0; i<aln_->core.n_cigar; ++i)
	{
		output << CigarOp { (int)bam_cigar_op(cigar[i]), (int)bam_cigar_oplen(cigar[i]) };
	}

	return output;
}

void BamAlignment::setCigarData(const QList<CigarOp>& cigar)
{
	//create new CIGAR datastructur for htslib
	const unsigned int n = cigar.size();
	uint32_t* cigar2 = new uint32_t[n];
	for (unsigned int i=0; i<n; ++i)
	{
		cigar2[i] = bam_cigar_gen(cigar[i].Length, cigar[i].Type);
	}

	//write new cigar string
	if (n != aln_->core.n_cigar)
	{
		int o = aln_->core.l_qname + aln_->core.n_cigar * 4;
		if (aln_->l_data + (n - aln_->core.n_cigar) * 4 > aln_->m_data) {
			aln_->m_data = aln_->l_data + (n - aln_->core.n_cigar) * 4;
			kroundup32(aln_->m_data);
			aln_->data = (uint8_t*)realloc(aln_->data, aln_->m_data);
		}
		memmove(aln_->data + aln_->core.l_qname + n * 4, aln_->data + o, aln_->l_data - o);
		memcpy(aln_->data + aln_->core.l_qname, cigar2, n * 4);
		aln_->l_data += (n - aln_->core.n_cigar) * 4;
		aln_->core.n_cigar = n;
	}
	else
	{
		memcpy(aln_->data + aln_->core.l_qname, cigar2, n * 4);
	}

	//delete datastructure
	delete[] cigar2;
}

QByteArray BamAlignment::cigarDataAsString(bool expand) const
{
	QByteArray output;

	const auto cigar = bam_get_cigar(aln_);
	for (uint32_t i = 0; i<aln_->core.n_cigar; ++i)
	{
		if (expand)
		{
			int len = bam_cigar_oplen(cigar[i]);
			char base = bam_cigar_opchr(cigar[i]);
			for (int j=0; j<len; ++j)
			{
				output.append(base);
			}
		}
		else
		{
			output.append(QString::number(bam_cigar_oplen(cigar[i])));
			output.append(bam_cigar_opchr(cigar[i]));
		}
	}

	return output;
}

QByteArray BamAlignment::bases() const
{
	QByteArray output;
	output.resize(aln_->core.l_qseq);

	uint8_t* s = bam_get_seq(aln_);
	for(int i=0; i<aln_->core.l_qseq; ++i)
	{
		output[i] = seq_nt16_str[bam_seqi(s, i)];
	}

	return output;
}

void BamAlignment::setBases(const QByteArray& bases)
{
	//check that length stays the same
	if (aln_->core.l_qseq!=bases.count())
	{
		THROW(NotImplementedException, "BamAlignment::setBases: Setting bases with different length is not implemented!");
	}

	//lookup table to convert char to index (revert seq_nt16_str[]="=ACMGRSVTWYHKDBN")
	static int char_to_base_index[256] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1,  1, -1,  2, -1, -1, -1,  4, -1, -1, -1, -1, -1, -1, 15, -1,
										  -1, -1, -1, -1,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1,  1, -1,  2, -1, -1, -1,  4, -1, -1, -1, -1, -1, -1, 15, -1,
										  -1, -1, -1, -1,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
										  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

	//process bases
	uint8_t* s = bam_get_seq(aln_);
	for(int i=0; i<bases.count(); ++i)
	{
		//determine base index
		int base_index = char_to_base_index[(int)(bases[i])];
		if (base_index==-1)
		{
			THROW(ProgrammingException, QByteArray("Cannot store character '") + bases[i] + "' in BAM file. Only A,C,G,T,N are allowed!");
		}

		//std::cout << "  base=" << bases[i] << " base_index=" << "(" << std::bitset<8>(base_index)  << ")" << std::endl;
		const int index = (i>>1); //two bases are stored on one byte => divide index by two via shifting
		uint8_t two_bases = s[index]; //two bases (lower~even index on the left)
		if (i % 2 == 0) //even index
		{
			//std::cout << "    index=" << i << " (even ~ left)" << std::endl;
			//std::cout << "    old=" << two_bases << "(" << std::bitset<8>(two_bases)  << ")" << std::endl;
			two_bases = (two_bases & 0xf) | (base_index << 4);
			//std::cout << "    new=" << two_bases << "(" << std::bitset<8>(two_bases)  << ")" << std::endl;
		}
		else //odd index
		{
			//std::cout << "    index=" << i << " (odd ~ right)" << std::endl;
			//std::cout << "    old=" << two_bases << "(" << std::bitset<8>(two_bases)  << ")" << std::endl;
			two_bases = (two_bases & 0xf0) | base_index;
			//std::cout << "    new=" << two_bases << "(" << std::bitset<8>(two_bases)  << ")" << std::endl;
		}
		s[index] = two_bases;
	}

	/*
	explaination how a base is retrieved by index 'i':
	int index = (i>>1); //two bases are stored on one byte => half index
	int shift = ((~(i)&1)<<2); //0 or 4 (for first/second half of the byte)
	int mask = 0xf; //00001111 (keeps second half of byte);
	int base_index = two_bases >> shift & mask;
	std::cout << "  base_index=" << base_index << "(" << std::bitset<8>(base_index)  << ")" << std::endl;
	std::cout << "  base=" << seq_nt16_str[base_index] << std::endl;
	*/
}

QByteArray BamAlignment::qualities() const
{
	QByteArray output;
	output.resize(aln_->core.l_qseq);

	uint8_t* q = bam_get_qual(aln_);
	for(int i=0; i<aln_->core.l_qseq; ++i)
	{
		output[i] = (char)(q[i]+33);
	}

	return output;
}

void BamAlignment::setQualities(const QByteArray& qualities)
{
	//check that length stays the same
	if (aln_->core.l_qseq!=qualities.count())
	{
		THROW(NotImplementedException, "BamAlignment::setQualities: Setting qualities with different length is not implemented!");
	}

	uint8_t* q = bam_get_qual(aln_);
	for(int i=0; i<qualities.count(); ++i)
	{
		q[i] = qualities[i]-33;
	}
}

QByteArray BamAlignment::tag(const QByteArray& tag) const
{
	uint8_t* data_raw = bam_aux_get(aln_, tag);
	if (data_raw==nullptr)
	{
		return QByteArray();
	}
	const char* data = reinterpret_cast<const char*>(data_raw);
	if (data[0]!='Z') THROW(NotImplementedException, "BamAlignment::tag: Getting tag data other than 'Z' type is not implemented!");
	return QByteArray(data);
}

int BamAlignment::tagi(const QByteArray& tag) const
{
	uint8_t* data_raw = bam_aux_get(aln_, tag);
	if (data_raw==nullptr)
	{
		return 0;
	}
	int64_t i = bam_aux2i(bam_aux_get(aln_, tag));
	return i;
}

void BamAlignment::addTag(const QByteArray& tag, char type, const QByteArray& value)
{
	if (bam_aux_append(aln_, tag, type, value.length()+1, reinterpret_cast<const unsigned char*>(value.data()))==-1)
	{
		THROW(FileAccessException, "Could not add tag '" + tag + "'' with value " + value + " to alignment.");
	}
}

QPair<char, int> BamAlignment::extractBaseByCIGAR(int pos)
{
	int read_pos = 0;
	int genome_pos = start()-1;
	const QList<CigarOp> cigar_data = cigarData();
	foreach(const CigarOp& op, cigar_data)
	{
		//update positions
		if (op.Type==BAM_CMATCH)
		{
			genome_pos += op.Length;
			read_pos += op.Length;
		}
		else if(op.Type==BAM_CINS)
		{
			read_pos += op.Length;
		}
		else if(op.Type==BAM_CDEL)
		{
			genome_pos += op.Length;

			//base is deleted
			if (genome_pos>=pos) return qMakePair('-', 255);
		}
		else if(op.Type==BAM_CREF_SKIP) //skipped reference bases (for RNA)
		{
			genome_pos += op.Length;

			//base is skipped
			if (genome_pos>=pos) return qMakePair('~', -1);
		}
		else if(op.Type==BAM_CSOFT_CLIP) //soft-clipped (only at the beginning/end)
		{
			read_pos += op.Length;

			//base is soft-clipped
			//Nb: reads that are mapped in paired end mode and completely soft-clipped (e.g. 7I64S, 71S) keep their original left-most (genomic) position
			if(read_pos>=length())	return qMakePair('~', -1);
		}
		else if(op.Type==BAM_CHARD_CLIP) //hard-clipped (only at the beginning/end)
		{
			//can be ignored as hard-clipped bases are not considered in the position or sequence
		}
		else
		{
			THROW(Exception, "Unknown CIGAR operation " + QString::number(op.Type) + "!");
		}

		if (genome_pos>=pos)
		{
			int actual_pos = read_pos - (genome_pos + 1 - pos);
			return qMakePair(base(actual_pos), quality((actual_pos)));
		}
	}

	THROW(Exception, "Could not find position  " + QString::number(pos) + " in read " + bases() + " with start position " + QString::number(start()) + "!");
}

QList<QByteArray> BamAlignment::extractIndelsByCIGAR(int pos, int indel_window)
{
	//init
	QList<QByteArray> output;
	bool use_window = (indel_window!=0);
	int window_start = pos - indel_window;
	int window_end = pos + indel_window;

	//look up indels
	int read_pos = 0;
	int genome_pos = start();
	const QList<CigarOp> cigar_data = cigarData();
	const QByteArray sequence = bases();
	foreach(const CigarOp& op, cigar_data)
	{
		//update positions
		if (op.Type==BAM_CMATCH) //match or mismatch
		{
			genome_pos += op.Length;
			read_pos += op.Length;
		}
		else if(op.Type==BAM_CINS) //insert
		{
			if ((!use_window && genome_pos==pos) || (use_window && genome_pos>=window_start && genome_pos<=window_end))
			{
				output.append("+" + sequence.mid(read_pos, op.Length));
			}

			read_pos += op.Length;
		}
		else if(op.Type==BAM_CDEL) //deletion
		{
			if ((!use_window && genome_pos==pos) || (use_window && genome_pos>=window_start && genome_pos<=window_end))
			{
				output.append("-" + QByteArray::number(op.Length));
			}
			genome_pos += op.Length;
		}
		else if(op.Type==BAM_CREF_SKIP) //skipped reference bases (for RNA)
		{
			genome_pos += op.Length;
		}
		else if(op.Type==BAM_CSOFT_CLIP) //soft-clipped (only at the beginning/end)
		{
			read_pos += op.Length;
		}
		else if(op.Type==BAM_CHARD_CLIP) //hard-clipped (only at the beginning/end)
		{
			//can be ignored as hard-clipped bases are not considered in the position or sequence
		}
		else
		{
			THROW(Exception, "Unknown CIGAR operation " + QString::number(op.Type) + "!");
		}

		//abort if we are behind the indel position
		if ((!use_window && genome_pos>pos) || (use_window && genome_pos>window_end)) break;
	}

	return output;
}

BamReader::BamReader(const QString& bam_file)
	: bam_file_(QFileInfo(bam_file).absoluteFilePath())
	, fp_(hts_open(bam_file.toLatin1().constData(), "r"))
{
	//open file
	if (fp_==nullptr)
	{
		THROW(FileAccessException, "Could not open BAM file " + bam_file_);
	}

	//read header
	header_ = bam_hdr_read(fp_->fp.bgzf);
	if (header_==nullptr)
	{
		THROW(FileAccessException, "Could not read header from BAM file " + bam_file);
	}

	//parse chromosome names and sizes
	for(int i=0; i<header_->n_targets; ++i)
	{
		Chromosome chr(header_->target_name[i]);
		chrs_ << chr;
		chrs_sizes_[chr] = header_->target_len[i];
	}
}

BamReader::~BamReader()
{
	clearIterator();
	hts_idx_destroy(index_);
	bam_hdr_destroy(header_);
	hts_close(fp_);
}

QByteArrayList BamReader::headerLines() const
{
	QByteArrayList output = QByteArray(header_->text).split('\n');

	//trim
	std::for_each(output.begin(), output.end(), [](QByteArray& line){ line = line.trimmed(); });

	return output;
}

void BamReader::setRegion(const Chromosome& chr, int start, int end)
{
	//clear data from previous calls
	clearIterator();

	//load index if not done already
	if (index_==nullptr)
	{
		index_ = bam_index_load(bam_file_.toLatin1().data());
		if (index_==nullptr)
		{
			THROW(FileAccessException, "Could not load index of BAM file " + bam_file_);
		}
	}

	//find chromosome string used in BAM header ('chr1' does not equal '1' for htslib)
	int chr_index = chrs_.indexOf(chr);
	if (chr_index==-1)
	{
		THROW(FileAccessException, "Could not find chromosome '" + chr.str() + "' in BAM file " + bam_file_);
	}

	//create iterator for region
	iter_ = bam_itr_queryi(index_, chr_index, start-1, end);
	if (iter_==nullptr)
	{
		QByteArray region_str = chrs_[chr_index].str() + ":" + QByteArray::number(start) + "-" + QByteArray::number(end);
		QByteArray extra;
		foreach(const Chromosome& chr, chrs_)
		{
			extra += chr.str();
		}
		THROW(FileAccessException, "Could not create iterator for region query " + region_str + " in BAM file " + bam_file_ + extra);
	}
}

bool BamReader::getNextAlignment(BamAlignment& al)
{
	int res = (iter_!=nullptr) ? bam_itr_next(fp_, iter_, al.aln_) : bam_read1(fp_->fp.bgzf, al.aln_);

	if (res<-1)
	{
		THROW(FileAccessException, "Could not read next alignment in BAM file " + bam_file_);
	}

	return res>=0;
}

const QList<Chromosome>& BamReader::chromosomes() const
{
	return chrs_;
}

const Chromosome& BamReader::chromosome(int chr_id) const
{
	if (chr_id>=chrs_.size()) THROW(ArgumentException, "Chromosome ID '" + QString::number(chr_id) + "' out of bounds in BAM file " + bam_file_);

	return chrs_[chr_id];
}

int BamReader::chromosomeSize(const Chromosome& chr) const
{
	if (!chrs_sizes_.contains(chr)) THROW(ArgumentException, "Chromosome '" + chr.str() + "' not known in BAM file " + bam_file_);

	return chrs_sizes_[chr];
}

double BamReader::genomeSize(bool nonspecial_only) const
{
	double sum = 0.0;
	foreach(const Chromosome& c, chrs_)
	{
		if (!nonspecial_only || c.isNonSpecial())
		{
			sum += chromosomeSize(c);
		}
	}
	return sum;
}

void BamReader::clearIterator()
{
	bam_itr_destroy(iter_);

	iter_ = nullptr;
}

Pileup BamReader::getPileup(const Chromosome& chr, int pos, int indel_window, int min_mapq, bool anom, int min_baseq)
{
	//init
	Pileup output;
	int reads_mapped = 0;
	int reads_mapq0 = 0;

	//restrict region
	setRegion(chr, pos, pos);

	//iterate through all alignments and create counts
	BamAlignment al;
	while (getNextAlignment(al))
	{
		if (!al.isProperPair() && anom==false) continue;
		if (al.isSecondaryAlignment()) continue;
		if (al.isDuplicate()) continue;
		if (al.isUnmapped()) continue;

		reads_mapped += 1;
		if (al.mappingQuality()==0) reads_mapq0 += 1;

		if (al.mappingQuality()<min_mapq) continue;

		//snps
		QPair<char, int> base = al.extractBaseByCIGAR(pos);
		if (base.second>=min_baseq)
		{
			output.inc(base.first);
		}

		//indels
		if (indel_window>=0)
		{
			output.addIndels(al.extractIndelsByCIGAR(pos, indel_window));
		}
	}

	output.setMapq0Frac((double)reads_mapq0 / reads_mapped);

	return output;
}


VariantDetails BamReader::getVariantDetails(const FastaFileIndex& reference, const Variant& variant)
{
	VariantDetails output;

	if (variant.isSNV()) //SVN
	{
		Pileup pileup = getPileup(variant.chr(), variant.start(), -1);
		output.depth = pileup.depth(true);
		if (output.depth!=0)
		{
			output.frequency = pileup.countOf(variant.obs()[0]) / (double)output.depth;
		}
		output.mapq0_frac = pileup.mapq0Frac();
	}
	else //indel
	{
		//determine region of interest for indel (important for repeat regions where more than one alignment is possible)
		QPair<int, int> reg = Variant::indelRegion(variant.chr(), variant.start(), variant.end(), variant.ref(), variant.obs(), reference);
		//qDebug() << "VAR:" << variant;
		//qDebug() << "REG:" << reg.first << reg.second;

		//get indels from region
		QVector<Sequence> indels;
		getIndels(reference, variant.chr(), reg.first-1, reg.second+1, indels, output.depth, output.mapq0_frac);
		//qDebug() << "INDELS:" << indels.join(" ");

		Variant variant_normalized = variant;
		variant_normalized.normalize("-");

		//count indels
		if (variant_normalized.ref()!="-" && variant_normalized.obs()!="-")
		{
			int c_ins = 0;
			int c_del = 0;
			foreach(const Sequence& indel, indels)
			{
				if (indel[0]=='+') ++c_ins;
				else if (indel[0]=='-') ++c_del;

			}
			output.frequency = std::min(c_ins, c_del);
		}
		else if (variant_normalized.ref()=="-")
		{
			output.frequency = indels.count("+" + variant_normalized.obs());
		}
		else
		{
			output.frequency = indels.count("-" + variant_normalized.ref());
		}

		//we might count more indels than depth because of the window - correct that
		output.frequency = std::min(1.0, output.frequency / output.depth);
	}

	return output;
}


void BamReader::getIndels(const FastaFileIndex& reference, const Chromosome& chr, int start, int end, QVector<Sequence>& indels, int& depth, double& mapq0_frac)
{
	//init
	indels.clear();
	depth = 0;
	int reads_mapped = 0;
	int reads_mapq0 = 0;

	//restrict region
	setRegion(chr, start, end);

	//iterate through all alignments and create counts
	BamAlignment al;
	while (getNextAlignment(al))
	{
		//skip low-quality reads
		if (al.isDuplicate()) continue;
		if (!al.isProperPair()) continue;
		if (al.isSecondaryAlignment()) continue;
		if (al.isUnmapped()) continue;

		reads_mapped += 1;
		if (al.mappingQuality()==0)
		{
			reads_mapq0 += 1;
			continue;
		}

		//skip reads that do not span the whole region
		if (al.start()>start || al.end()<end ) continue;
		++depth;

		//run time optimization: skip reads that do not contain Indels
		bool contains_indels = false;
		const QList<CigarOp> cigar_data = al.cigarData();
		foreach(const CigarOp& op, cigar_data)
		{
			if (op.Type==BAM_CINS || op.Type==BAM_CDEL)
			{
				contains_indels = true;
				break;
			}
		}
		if (!contains_indels) continue;

		//look up indels
		int read_pos = 0;
		int genome_pos = al.start();
		const QList<CigarOp> cigar_data2 = al.cigarData();
		foreach(const CigarOp& op, cigar_data2)
		{
			//update positions
			if (op.Type==BAM_CMATCH)
			{
				genome_pos += op.Length;
				read_pos += op.Length;
			}
			else if(op.Type==BAM_CINS)
			{
				if (genome_pos>=start && genome_pos<=end)
				{
					indels.append(QByteArray("+") + al.bases().mid(read_pos, op.Length));
				}
				read_pos += op.Length;
			}
			else if(op.Type==BAM_CDEL)
			{
				if (genome_pos>=start && genome_pos<=end)
				{
					indels.append("-" + reference.seq(chr.str(), genome_pos, op.Length));
				}
				genome_pos += op.Length;
			}
			else if(op.Type==BAM_CREF_SKIP) //skipped reference bases (for RNA)
			{
				genome_pos += op.Length;
			}
			else if(op.Type==BAM_CSOFT_CLIP) //soft-clipped (only at the beginning/end)
			{
				read_pos += op.Length;
			}
			else if(op.Type==BAM_CHARD_CLIP) //hard-clipped (only at the beginning/end)
			{
				//can be ignored as hard-clipped bases are not considered in the position or sequence
			}
			else
			{
				THROW(Exception, "Unknown CIGAR operation " + QString::number(op.Type) + "!");
			}
		}
	}

	mapq0_frac = (double)reads_mapq0 / reads_mapped;
}
