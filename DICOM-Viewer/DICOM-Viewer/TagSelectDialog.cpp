#include "TagSelectDialog.h"

TagSelectDialog::TagSelectDialog(QDialog * parent)
{
	ui.setupUi(this);
	ui.tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui.tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	ui.tableWidget->verticalHeader()->setDefaultSectionSize(12);
	ui.tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	QHeaderView *verticalHeader = ui.tableWidget->verticalHeader();
	verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
	verticalHeader->setDefaultSectionSize(10);
}

//========================================================================================================================
TagSelectDialog::~TagSelectDialog()
{
	clearTable();
}

//========================================================================================================================
DcmWidgetElement TagSelectDialog::getElement() const
{
	return this->element;
}

//========================================================================================================================
void TagSelectDialog::clearTable() const
{
	for (int i = ui.tableWidget->rowCount() - 1; i >= 0; i--)
	{
		ui.tableWidget->removeRow(i);
	}
}

//========================================================================================================================
void TagSelectDialog::populate() const
{
	auto* dictionary = new DcmDataDictionary(true,false);
	DcmHashDictIterator iterStart = dictionary->normalBegin();
	const DcmHashDictIterator iterEnd = dictionary->normalEnd();
	int count = 0;

	while (iterStart != iterEnd)
	{
		const DcmDictEntry* item = *iterStart;
		QString key = item->getBaseTag().toString().c_str();
		QString description = item->getTagName();
		QString vr = item->getVR().getVRName();

		if (key != "(fffe,e00d)" && key != "(fffe,00dd)")
		{
			ui.tableWidget->insertRow(count);
			ui.tableWidget->setItem(count, 0, new QTableWidgetItem(key));
			ui.tableWidget->setItem(count, 1, new QTableWidgetItem(description));
			ui.tableWidget->setItem(count, 2, new QTableWidgetItem(vr));
			count++;
		}

		++iterStart;
	}

	ui.tableWidget->resizeColumnsToContents();
	delete dictionary;
}

//========================================================================================================================
void TagSelectDialog::okPressed()
{
	QList<QTableWidgetItem*> items = ui.tableWidget->selectedItems();

	if (!items.empty())
	{
		const DcmWidgetElement element = DcmWidgetElement(items[0]->text(), items[2]->text(), "", "", items[1]->text(), ui.lineEdit->text());

		if (element.getItemValue().isEmpty() && element.getItemVR() != "na" && element.getItemVR() != "SQ")
		{
			auto* box = new QMessageBox();
			box->setText("No value entered!");
			box->setIcon(QMessageBox::Warning);
			box->exec();
			delete box;
			this->reject();
		}

		this->element = element;
		this->accept();
	}
}

//========================================================================================================================
void TagSelectDialog::cancelPressed()
{
	this->reject();
}

//========================================================================================================================
void TagSelectDialog::findText() const
{
	const QString text = ui.lineEditSearch->text();
	std::vector<DcmWidgetElement> result;

	if (!text.isEmpty())
	{
		std::unique_ptr <DcmDataDictionary> dictionary = std::make_unique<DcmDataDictionary>(true, false);
		DcmHashDictIterator iterStart = dictionary->normalBegin();
		const DcmHashDictIterator iterEnd = dictionary->normalEnd();

		while (iterStart != iterEnd)
		{
			const DcmDictEntry* item = *iterStart;
			QString key = item->getBaseTag().toString().c_str();
			QString description = item->getTagName();
			QString vr = item->getVR().getVRName();

			DcmWidgetElement element = DcmWidgetElement(key, vr, "", "", description, "");
			if (element.checkIfContains(text))
			{
				result.push_back(element);
			}

			++iterStart;
		}

		if (!result.empty())
		{
			clearTable();

			for (unsigned long i = 0; i < result.size(); i++)
			{
				if (result[i].getItemTag() != "(fffe,e00d)" &&result[i].getItemTag() != "(fffe,00dd)")
				{
					ui.tableWidget->insertRow(i);
					ui.tableWidget->setItem(i, 0, new QTableWidgetItem(result[i].getItemTag()));
					ui.tableWidget->setItem(i, 1, new QTableWidgetItem(result[i].getItemDescription()));
					ui.tableWidget->setItem(i, 2, new QTableWidgetItem(result[i].getItemVR()));
				}
			}
		}
	}

	else
	{
		clearTable();
		populate();
	}
}
