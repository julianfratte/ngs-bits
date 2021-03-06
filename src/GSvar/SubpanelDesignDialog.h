#ifndef SUBPANELDESIGNDIALOG_H
#define SUBPANELDESIGNDIALOG_H

#include <QDialog>
#include <QCompleter>
#include "BedFile.h"
#include "GeneSet.h"

namespace Ui {
class SubpanelDesignDialog;
}

class SubpanelDesignDialog
	: public QDialog
{
	Q_OBJECT

public:
	explicit SubpanelDesignDialog(QWidget *parent = 0);
	~SubpanelDesignDialog();

	///Sets the gene list
	void setGenes(const GeneSet& genes);

	///Returns the last created subpane name (or an empty string if not subpanel was designed).
	QString lastCreatedSubPanel();

protected slots:
	void checkAndCreatePanel();
	void storePanel();
	void disableStoreButton();
	void importFromExistingSubpanel();

private:
	QStringList subpanelList();
	void createSubpanelCompleter();
	QString getBedFilename() const;
	QString getBedFilenameArchive() const;
	QString getBedSuffix() const;

	void clearMessages();
	void addMessage(QString text, bool is_error, bool update_gui);
	bool errorMessagesPresent();

	struct Message
	{
		QString text;
		bool is_error;
	};
	QList<Message> messages;
	Ui::SubpanelDesignDialog* ui;
	QCompleter* completer;
	GeneSet genes;
	BedFile regions;
	QString roi_file;
	QString gene_file;
	QString last_created_subpanel;
};

#endif // SUBPANELDESIGNDIALOG_H
