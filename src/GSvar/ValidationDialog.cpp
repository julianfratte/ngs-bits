#include "ValidationDialog.h"
#include "NGSD.h"
#include "Helper.h"
#include "BasicStatistics.h"
#include <QStringList>

//TODO remove variant_validation/type from NGSD schema, once the deployed version of GSvar can work without it > MARC

ValidationDialog::ValidationDialog(QWidget* parent, int id)
	: QDialog(parent)
	, ui_()
	, db_()
	, val_id_(QString::number(id))
{
	ui_.setupUi(this);

	//set up UI
	connect(ui_.status, SIGNAL(currentTextChanged(QString)), this, SLOT(statusChanged()));
	QStringList status = db_.getEnum("variant_validation", "status");
	foreach(QString s, status)
	{
		ui_.status->addItem(s);
	}

	//fill with data
	SqlQuery query = db_.getQuery();
	query.exec("SELECT * FROM variant_validation WHERE id=" + val_id_);
	query.next();

	Variant variant = db_.variant(query.value("variant_id").toString());
	ui_.variant->setText(variant.toString() + " (" + query.value("genotype").toString() + ")");

	ui_.sample->setText(db_.getValue("SELECT name FROM sample WHERE id=" + query.value("sample_id").toString()).toString());

	ui_.requested_by->setText(db_.getValue("SELECT name FROM user WHERE id=" + query.value("user_id").toString()).toString());

	ui_.status->setCurrentText(query.value("status").toString());

	ui_.comment->setPlainText(query.value("comment").toString());
}

void ValidationDialog::store()
{
	SqlQuery query = db_.getQuery(); //use binding (user input)
	query.prepare("UPDATE variant_validation SET status=:0, comment=:1 WHERE id='" + val_id_ + "'");
	query.bindValue(0, ui_.status->currentText());
	query.bindValue(1, ui_.comment->toPlainText().trimmed());
	query.exec();
}

QString ValidationDialog::status()
{
	return ui_.status->currentText();
}

void ValidationDialog::statusChanged()
{
	QString text = ui_.comment->toPlainText().trimmed();
	if (text!="") text += "\n";
	text += "[" + ui_.status->currentText() + "] " + Helper::userName() + " " + QDate::currentDate().toString("dd.MM.yyyy");
	ui_.comment->setPlainText(text);
}
