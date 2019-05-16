#ifndef VARIANTTABLE_H
#define VARIANTTABLE_H

#include <QTableWidget>
#include "GeneSet.h"
#include "FilterCascade.h"

//GUI representation of (filtered) variant table
class VariantTable
	: public QTableWidget
{
	Q_OBJECT

public:
	VariantTable(QWidget* parent);

	///Update table
	void update(const VariantList variants_, const FilterResult& filter_result_, const GeneSet& imprinting_genes_, int max_variants);

	///Returns the column index, or -1 if not found.
	int columnIndex(const QString& column_name) const;

	///Returns the current variant index, or -1 if no/several variants are selected. If @p gui_indices is true, GUI table indices are returned instead of variant list index.
	int selectedVariantIndex(bool gui_indices = false) const;

	///Returns a sorted list of selected variants indices in the variant list. If @p gui_indices is true, GUI table indices are returned instead of variant list index.
	QList<int> selectedVariantsIndices(bool gui_indices = false) const;

	///Convert table row to variant index.
	int rowToVariantIndex(int row) const;

	///Creates table widget items, or nullptr if the text is empty. Uses Qt implicit sharing to avoid duplicate strings.
	QTableWidgetItem* createTableItem(const QString& text) const
	{
		//no text > no item (optimization for WGS - empty items are big and take a lot of RAM)
		if (text.isEmpty()) return nullptr;

		return new QTableWidgetItem(text);
	}

	///Returns the current column widths.
	QList<int> columnWidths() const;

	///Sets column widths.
	void setColumnWidths(const QList<int>& widths);

public slots:

	///Clear contents
	void clearContents();

	///Set the row heights
	void adaptRowHeights();

	///Resize table cells for better readability.
	void adaptColumnWidths();

	///Resize table cells for better readability (custom).
	void adaptColumnWidthsCustom();

	///Copy table to clipboard
	void copyToClipboard(bool split_quality=false);

protected:

	///Override copy command
	void keyPressEvent(QKeyEvent* event) override;
};

#endif // VARIANTTABLE_H