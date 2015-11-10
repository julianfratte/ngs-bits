
-- table `gene`
INSERT INTO `gene` (`id`, `hgnc_id`, `symbol`, `name`, `chromosome`, `type`) VALUES
(1, 1101, 'BRCA2', 'breast cancer 2, early onset', '13', 'protein-coding gene');

-- table `gene_transcript`
INSERT INTO `gene_transcript` (`id`, `gene_id`, `name`, `source`, `start_coding`, `end_coding`, `strand`) VALUES
(1, 1, 'uc001uua.1', 'ucsc', 32899266, 32907523, '+'),
(2, 1, 'uc001uub.1', 'ucsc', 32890598, 32972907, '+'),
(3, 1, 'uc031qky.1', 'ucsc', 32929167, 32936796, '+'),
(4, 1, 'uc031qkz.1', 'ucsc', 32945093, 32945092, '+'),
(5, 1, 'CCDS9344.1', 'ccds', 32890598, 32972907, '+');

-- table `gene_exon`
INSERT INTO `gene_exon` (`transcript_id`, `start`, `end`) VALUES
(1, 32889617, 32889804),
(1, 32890559, 32890660),
(1, 32893214, 32893462),
(1, 32899213, 32899321),
(1, 32900238, 32900287),
(1, 32900379, 32900419),
(1, 32900636, 32900750),
(1, 32903580, 32903629),
(1, 32905056, 32905167),
(1, 32906409, 32907524),
(2, 32889617, 32889804),
(2, 32890559, 32890664),
(2, 32893214, 32893462),
(2, 32899213, 32899321),
(2, 32900238, 32900287),
(2, 32900379, 32900419),
(2, 32900636, 32900750),
(2, 32903580, 32903629),
(2, 32905056, 32905167),
(2, 32906409, 32907524),
(2, 32910402, 32915333),
(2, 32918695, 32918790),
(2, 32920964, 32921033),
(2, 32928998, 32929425),
(2, 32930565, 32930746),
(2, 32931879, 32932066),
(2, 32936660, 32936830),
(2, 32937316, 32937670),
(2, 32944539, 32944694),
(2, 32945093, 32945237),
(2, 32950807, 32950928),
(2, 32953454, 32953652),
(2, 32953887, 32954050),
(2, 32954144, 32954282),
(2, 32968826, 32969070),
(2, 32971035, 32971181),
(2, 32972299, 32973809),
(3, 32928998, 32929425),
(3, 32936660, 32936830),
(4, 32945093, 32945237),
(4, 32953454, 32953652),
(5, 32890598, 32890664),
(5, 32893214, 32893462),
(5, 32899213, 32899321),
(5, 32900238, 32900287),
(5, 32900379, 32900419),
(5, 32900636, 32900750),
(5, 32903580, 32903629),
(5, 32905056, 32905167),
(5, 32906409, 32907524),
(5, 32910402, 32915333),
(5, 32918695, 32918790),
(5, 32920964, 32921033),
(5, 32928998, 32929425),
(5, 32930565, 32930746),
(5, 32931879, 32932066),
(5, 32936660, 32936830),
(5, 32937316, 32937670),
(5, 32944539, 32944694),
(5, 32945093, 32945237),
(5, 32950807, 32950928),
(5, 32953454, 32953652),
(5, 32953887, 32954050),
(5, 32954144, 32954282),
(5, 32968826, 32969070),
(5, 32971035, 32971181),
(5, 32972299, 32972907);