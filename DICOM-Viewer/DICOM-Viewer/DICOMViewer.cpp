#include "DICOMViewer.h"
#include <fstream>

#define SPACE "  "

DICOMViewer::DICOMViewer(QWidget *parent) : QMainWindow(parent)
{
	ui.setupUi(this);
	ui.tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui.tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	ui.tableWidget->verticalHeader()->setDefaultSectionSize(12);
	ui.tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	ui.buttonDelete->setEnabled(false);
	ui.buttonEdit->setEnabled(false);
	ui.buttonInsert->setEnabled(false);
}

//========================================================================================================================
void DICOMViewer::fileTriggered(QAction* qaction)
{
	QString option = qaction->text();

	if (option == "Open")
	{
		QString fileName = QFileDialog::getOpenFileName(this,tr("Open File"), tr(""), tr("DICOM File (*.dcm)"));

		if (!fileName.isEmpty())
		{
			if (file.loadFile(fileName.toStdString().c_str()).good())
			{
				ui.tableWidget->scrollToTop();
				this->clearTable();
				this->extractData(file);
				ui.buttonDelete->setEnabled(true);
				ui.buttonEdit->setEnabled(true);
				ui.buttonInsert->setEnabled(true);
				ui.tableWidget->resizeColumnsToContents();
				this->setWindowTitle("Viewer - " + fileName);
				ui.label->setText("Size: " + QString::fromStdString(std::to_string(getFileSize(fileName.toStdString())) + " MB"));
			}
			else
			{
				this->alertFailed("Failed to open file!");
			}
		}
	}

	else if(option == "Close")
	{
		this->clearTable();
	}

	else if (option == "Compare")
	{
		CompareDialog* dialog = new CompareDialog(nullptr);
		dialog->exec();
		delete dialog;
	}

	else if (option == "Save as")
	{
		QString fileName = QFileDialog::getSaveFileName(this,tr("Save File"),tr(""), tr("DICOM File (*.dcm)"));
		if (!fileName.isEmpty())
		{
			if (!file.saveFile(fileName.toStdString().c_str()).good())
			{
				alertFailed("Failed to save!");
			}
		}
	}
}

//========================================================================================================================
void DICOMViewer::extractData(DcmFileFormat file)
{
	DcmMetaInfo* metaInfo = file.getMetaInfo();
	DcmDataset* dataSet = file.getDataset();

	for (int i = 0; i < metaInfo->card(); i++)
	{
		this->insertInTable(metaInfo->getElement(i));
	}

	for (int i = 0; i < dataSet->card(); i++)
	{
		this->insertInTable(dataSet->getElement(i));
	}
}

//========================================================================================================================
void DICOMViewer::insertInTable(DcmElement* element)
{
	this->depthRE = 0;
	this->getNestedSequences(DcmTagKey(
		OFstatic_cast(Uint16, element->getGTag()),
		OFstatic_cast(Uint16, element->getETag())),nullptr);

	if (this->nestedElements.size())
	{
		for (auto element : this->nestedElements)
		{
			this->indent(element, element.getDepth());

			this->insert(element, globalIndex);

			DcmWidgetElement copyElement = DcmWidgetElement(element);
			copyElement.setTableIndex(globalIndex);
			this->elements.push_back(copyElement);
			this->globalIndex++;

		}

		this->nestedElements.clear();
	}
	
	else
	{
		DcmWidgetElement widgetElement = createElement(element,nullptr,nullptr);
		widgetElement.setItemTag(widgetElement.getItemTag().toUpper());
		this->insert(widgetElement, globalIndex);
		DcmWidgetElement copyElement = DcmWidgetElement(widgetElement);
		copyElement.setTableIndex(globalIndex);
		this->elements.push_back(copyElement);
		this->globalIndex++;
	}
}

//========================================================================================================================
void DICOMViewer::repopulate(std::vector<DcmWidgetElement> source)
{
	ui.tableWidget->clearContents();
	for (int i = ui.tableWidget->rowCount(); i >= 0; i--)
	{
		ui.tableWidget->removeRow(i);
	}

	for (int i = 0; i < source.size(); i++)
	{
		DcmWidgetElement element = DcmWidgetElement(source[i]);

		this->insert(element, i);
	}
}

//========================================================================================================================
void DICOMViewer::getNestedSequences(DcmTagKey tag, DcmSequenceOfItems* sequence)
{
	OFCondition cond = OFCondition(EC_CorruptedData);
	if (tag.hasValidGroup())
		cond = file.getDataset()->findAndGetSequence(tag, sequence, true);

	if (cond.good() || (sequence != nullptr &&  sequence->card()))
	{
		DcmWidgetElement widgetElement1 = createElement(nullptr, sequence, nullptr);
		widgetElement1.setDepth(this->depthRE);
		widgetElement1.setItemTag(widgetElement1.getItemTag().toUpper());
		this->nestedElements.push_back(widgetElement1);
		this->depthRE++;

		for (int i = 0; i < sequence->card(); i++)
		{
			DcmWidgetElement widgetElement2 = createElement(nullptr, nullptr, sequence->getItem(i));
			widgetElement2.setDepth(this->depthRE);
			widgetElement2.setItemTag(widgetElement2.getItemTag().toUpper());
			this->nestedElements.push_back(widgetElement2);
			this->iterateItem(sequence->getItem(i), this->depthRE);
			DcmWidgetElement widgetElementDelim = DcmWidgetElement(
				QString("(FFFE,E00D)"),
				QString("??"), QString("0"),
				QString("0"),
				QString("ItemDelimitationItem"),
				QString(""));
			this->depthRE--;
			widgetElementDelim.setDepth(this->depthRE);
			this->nestedElements.push_back(widgetElementDelim);
		}

		DcmWidgetElement widgetElementDelim = DcmWidgetElement(
			QString("(FFFE,E0DD)"),
			QString("??"),
			QString("0"),
			QString("0"),
			QString("SequenceDelimitationItem"),
			QString(""));
		this->depthRE--;
		widgetElementDelim.setDepth(this->depthRE);
		this->nestedElements.push_back(widgetElementDelim);
	}
}

//========================================================================================================================
void DICOMViewer::iterateItem(DcmItem * item,int& depth)
{
	depth++;

	for (int i = 0; i < item->getNumberOfValues(); i++)
	{
		DcmWidgetElement widgetElement = createElement(item->getElement(i), nullptr, nullptr);
		widgetElement.setItemTag(widgetElement.getItemTag().toUpper());

		if (widgetElement.getItemVR() != "SQ")
		{
			widgetElement.setDepth(depth);
			this->nestedElements.push_back(widgetElement);
		}

		DcmTagKey NULLkey;
		DcmSequenceOfItems* sequence;
		item->getElement(i)->getParentItem()->findAndGetSequence(item->getElement(i)->getTag().getBaseTag(), sequence, true);
		this->getNestedSequences(NULLkey, sequence);
	}
}

//========================================================================================================================
void DICOMViewer::clearTable()
{
	if (ui.tableWidget->rowCount())
	{
		this->nestedElements.clear();
		this->elements.clear();
		this->globalIndex = 0;

		for (int i = ui.tableWidget->rowCount(); i >= 0; i--)
		{
			ui.tableWidget->removeRow(i);
		}

		ui.lineEdit->clear();
		this->setWindowTitle("Viewer");
	}
}

//========================================================================================================================
void DICOMViewer::alertFailed(std::string message)
{
	QMessageBox* messageBox = new QMessageBox();
	messageBox->setIcon(QMessageBox::Warning);
	messageBox->setText(QString::fromStdString(message));
	messageBox->exec();
	delete messageBox;
}

//========================================================================================================================
void DICOMViewer::indent(DcmWidgetElement & element, int depth)
{
	QString str = element.getItemTag();

	while (depth--)
	{
		str.insert(0, SPACE);
	}

	element.setItemTag(str);
}

//========================================================================================================================
DcmWidgetElement DICOMViewer::createElement(DcmElement* element, DcmSequenceOfItems* sequence, DcmItem* item)
{
	if (element)
	{
		DcmTagKey tagKey = DcmTagKey(
			OFstatic_cast(Uint16, element->getGTag()), 
			OFstatic_cast(Uint16, element->getETag()));
		DcmTag tagName = DcmTag(tagKey);
		DcmVR vr = DcmVR(element->getVR());
		std::string str;

		for (int i = 0; i < 10; i++)
		{
			OFString value;
			element->getOFString(value, i, false);
			str.append(value.c_str());
			str.append(" ");
		}

		DcmWidgetElement widgetElement = DcmWidgetElement(
			QString::fromStdString(tagKey.toString().c_str()),
			QString::fromStdString(vr.getVRName()),
			QString::fromStdString(std::to_string(element->getVM())),
			QString::fromStdString(std::to_string(element->getLength())),
			tagName.getTagName(),
			QString::fromStdString(str));

		return widgetElement;
	}

	else if (sequence)
	{
		DcmTagKey tagKey = DcmTagKey(
			OFstatic_cast(Uint16, sequence->getGTag()), 
			OFstatic_cast(Uint16, sequence->getETag()));
		DcmTag tagName = DcmTag(tagKey);
		DcmVR vr = DcmVR(sequence->getVR());
		std::string str;

		for (int i = 0; i < 10; i++)
		{
			OFString value;
			sequence->getOFString(value, i, false);
			str.append(value.c_str());
			str.append(" ");
		}

		DcmWidgetElement widgetElement = DcmWidgetElement(
			QString::fromStdString(tagKey.toString().c_str()),
			QString::fromStdString(vr.getVRName()),
			QString::fromStdString(std::to_string(sequence->getVM())),
			QString::fromStdString(std::to_string(sequence->getLength())),
			tagName.getTagName(),
			QString::fromStdString(str));

		return widgetElement;
	}

	else if (item)
	{
		DcmTagKey tagKey = DcmTagKey(
			OFstatic_cast(Uint16, item->getGTag()),
			OFstatic_cast(Uint16, item->getETag()));
		DcmTag tagName = DcmTag(tagKey);
		DcmVR vr = DcmVR(item->getVR());

		DcmWidgetElement widgetElement = DcmWidgetElement(
			QString::fromStdString(tagKey.toString().c_str()),
			QString::fromStdString(vr.getVRName()),
			QString::fromStdString(std::to_string(item->getVM())),
			QString::fromStdString(std::to_string(item->getLength())),
			tagName.getTagName(),
			QString::fromStdString(""));

		return widgetElement;
	}

	else
	{
		return DcmWidgetElement();
	}
}

//========================================================================================================================
void DICOMViewer::insert(DcmWidgetElement element, int &index)
{
	ui.tableWidget->insertRow(index);
	ui.tableWidget->setItem(index, 0, new QTableWidgetItem(element.getItemTag()));
	ui.tableWidget->setItem(index, 1, new QTableWidgetItem(element.getItemVR()));
	ui.tableWidget->setItem(index, 2, new QTableWidgetItem(element.getItemVM()));
	ui.tableWidget->setItem(index, 3, new QTableWidgetItem(element.getItemLength()));
	ui.tableWidget->setItem(index, 4, new QTableWidgetItem(element.getItemDescription()));
	ui.tableWidget->setItem(index, 5, new QTableWidgetItem(element.getItemValue()));
}

//========================================================================================================================
double DICOMViewer::getFileSize(std::string fileName)
{
	std::ifstream in_file(fileName, std::ios::binary | std::ios::ate);
	double size = in_file.tellg() / 1024;
	size /= 1024;
	in_file.close();
	return size;
}

//========================================================================================================================
void DICOMViewer::getTagKeyOfSequence(DcmTagKey key, int row, DcmTagKey* returnKey, int* numberInSequence)
{
	*numberInSequence = 0;

	for (int i = row - 1; i >= 0; i--)
	{
		DcmWidgetElement element = DcmWidgetElement(
			ui.tableWidget->item(i, 0)->text(),
			ui.tableWidget->item(i, 1)->text(),
			ui.tableWidget->item(i, 2)->text(),
			ui.tableWidget->item(i, 3)->text(),
			ui.tableWidget->item(i, 4)->text(),
			ui.tableWidget->item(i, 5)->text());

		if (element.getItemVR() == "na" && element.getItemDescription() == "Item")
		{
			(*numberInSequence)++;
		}

		if (element.getItemVR() == "SQ")
		{
			*returnKey = element.extractTagKey();
			break;
		}
	}
}

//========================================================================================================================
void DICOMViewer::findText()
{
	QString text = ui.lineEdit->text();

	if (!text.isEmpty())
	{
		if (this->elements.size())
		{
			std::vector<DcmWidgetElement> result;

			for (DcmWidgetElement element : this->elements)
			{
				if (element.checkIfContains(text))
				{
					result.push_back(element);
				}
			}

			repopulate(result);
			ui.tableWidget->scrollToTop();
		}
	}

	else
	{
		repopulate(this->elements);
	}
}

//========================================================================================================================
void DICOMViewer::tableClicked(int row, int collumn)
{
	QList<QTableWidgetItem*> selectedTags = ui.tableWidget->selectedItems();

	DcmWidgetElement element = DcmWidgetElement(selectedTags[0]->text().trimmed(), selectedTags[1]->text(),
		selectedTags[2]->text(), selectedTags[3]->text(), selectedTags[4]->text(), selectedTags[5]->text());

	if (selectedTags.size())
	{
		for (auto elementV : this->elements)
		{
			if (element == elementV && !shouldModify(element))
			{
				ui.buttonEdit->setEnabled(false);
				ui.buttonDelete->setEnabled(false);
				return;
			}
		}
	}

	ui.buttonEdit->setEnabled(true);
	ui.buttonDelete->setEnabled(true);
}

//========================================================================================================================
void DICOMViewer::closeButtonClicked()
{
	this->close();
}

//========================================================================================================================
void DICOMViewer::editClicked()
{
	QList<QTableWidgetItem*> items = ui.tableWidget->selectedItems();

	if (items.size())
	{
		DcmWidgetElement elementWidget =  DcmWidgetElement(items[0]->text(), items[1]->text(), items[2]->text(), items[3]->text(), items[4]->text(), items[5]->text());
		this->createSimpleEditDialog(elementWidget);
	}
}

//========================================================================================================================
bool DICOMViewer::deleteElementFromFile(DcmSequenceOfItems* sequence, DcmWidgetElement element, QList<DcmWidgetElement> list)
{
	int count = -1;
	int i = list.size() - 1;

	while (i >= 0 && list[i].getItemVR() == "na")
	{
		count++;
		i--;
		list.removeLast();
	}

	if (list.size() == 0)
	{
		count--;

		if (!sequence->getParentItem()->findAndDeleteSequenceItem(sequence->getTag().getBaseTag(), count).good())
		{
			alertFailed("Failed!");
			return false;
		}

		return true;
	}

	DcmItem* item = sequence->getItem(count);

	if (list[i] == element)
	{
		if (!item->findAndDeleteElement(element.extractTagKey(), false, false).good())
		{
			alertFailed("Failed!");
			return false;
		}

		else
		{
			return true;
		}
	}

	else
	{
		DcmSequenceOfItems* seq;

		if (item->findAndGetSequence(list[i].extractTagKey(), seq, false, false).good())
		{
			list.removeLast();
			return deleteElementFromFile(seq, element, list);
		}

		else
		{
			alertFailed("Failed!");
			return false;
		}
	}
}

//========================================================================================================================
bool DICOMViewer::modifyValue(DcmSequenceOfItems* sequence, DcmWidgetElement element, QList<DcmWidgetElement> list, QString value)
{
	int count = -1;
	int i = list.size() - 1;

	while (list[i].getItemVR() == "na")
	{
		count++;
		i--;
		list.removeLast();
	}

	DcmItem* item = sequence->getItem(count);

	if (list[i] == element)
	{
		DcmElement* el;

		if (item->findAndGetElement(element.extractTagKey(), el, false, false).good())
		{
			if (el->putString(value.toStdString().c_str()).good())
			{
				return true;
			}

			else
			{
				alertFailed("Failed!");
				return false;
			}
		}

		else
		{
			alertFailed("Failed!");
			return false;
		}
	}

	else
	{
		DcmSequenceOfItems* seq;

		if (item->findAndGetSequence(list[i].extractTagKey(), seq, false, false).good())
		{
			list.removeLast();
			return modifyValue(seq, element, list, value);
		}

		else
		{
			alertFailed("Failed!");
			return false;
		}
	}
}

//========================================================================================================================
bool DICOMViewer::insertElement(DcmSequenceOfItems * sequence, DcmWidgetElement element, DcmWidgetElement insertElement, QList<DcmWidgetElement> list)
{
	int count = -1;
	int i = list.size() - 1;

	while (i >= 0 && list[i].getItemVR() == "na")
	{
		count++;
		i--;
		list.removeLast();
	}

	if (list.size() == 0)
	{
		count--;
		DcmItem* item = sequence->getItem(count);

		if (!item->putAndInsertString(insertElement.extractTagKey(), insertElement.getItemValue().toStdString().c_str(), false).good())
		{
			alertFailed("Failed!");
			return false;
		}

		return true;
	}

	DcmItem* item = sequence->getItem(count);

	if (list[i] == element)
	{
		if (insertElement.getItemVR() == "na")
		{
			DcmItem* it = new DcmItem(DcmTag(insertElement.extractTagKey()));
			if (!item->insertSequenceItem(element.extractTagKey(), it).good())
			{
				alertFailed("Failed!");
				return false;
			}

			return true;
		}

		else
		{
			if (!item->putAndInsertString(insertElement.extractTagKey(), insertElement.getItemValue().toStdString().c_str(), false).good())
			{
				alertFailed("Failed!");
				return false;
			}

			return true;
		}
	}

	else
	{
		DcmSequenceOfItems* seq;

		if (item->findAndGetSequence(list[i].extractTagKey(), seq, false, false).good())
		{
			list.removeLast();
			return this->insertElement(seq, element, insertElement, list);
		}

		else
		{
			alertFailed("Failed");
			return false;
		}
	}
}

//========================================================================================================================
void DICOMViewer::createSimpleEditDialog(DcmWidgetElement element)
{
	element.calculateDepthFromTag();
	EditDialogSimple* editDialog = new EditDialogSimple(nullptr);
	editDialog->setValue(element.getItemValue());
	editDialog->setWindowTitle(element.getItemVR());
	editDialog->setDescription(element.getItemDescription());
	editDialog->exec();

	element.calculateTableIndex(currentRow(element, ui.tableWidget->currentRow()), this->elements);

	QList<DcmWidgetElement> list;
	list.append(element);
	generatePathToRoot(element, element.getTableIndex(), &list);

	QString result = editDialog->getValue();

	if (!result.isEmpty())
	{
		if (list.size() > 1)
		{
			DcmSequenceOfItems* sequence;

			if(file.getDataset()->findAndGetSequence(list[list.size() - 1].extractTagKey(),sequence,false,false).good())
			{
				list.removeLast();

				if (modifyValue(sequence, element, list,result))
				{
					clearTable();
					extractData(file);
				}
			}
		}

		else
		{
			DcmElement* el;

			if (file.getDataset()->findAndGetElement(element.extractTagKey(), el, false, false).good())
			{
				if (el->putString(result.toStdString().c_str()).good())
				{
					clearTable();
					extractData(file);
				}

				else
				{
					alertFailed("Failed!");
				}
			}

			else
			{
				alertFailed("Failed!");
			}
		}
	}

	delete editDialog;
}

//========================================================================================================================
void DICOMViewer::generatePathToRoot(DcmWidgetElement element, int row, QList<DcmWidgetElement> *elements)
{
	element.calculateDepthFromTag();

	if (element.getDepth() == 0)
	{
		return;
	}

	DcmWidgetElement el = this->elements[row];
	el.calculateDepthFromTag();

	if (el.getItemVR() == "na" && el.getDepth() == element.getDepth())
	{
		(*elements).append(el);
	}

	if (el.getDepth() == element.getDepth() - 1)
	{
		(*elements).append(el);
		generatePathToRoot(el, --row, elements);
	}

	else
	{
		generatePathToRoot(element, --row, elements);
	}
}

//========================================================================================================================
bool DICOMViewer::shouldModify(DcmWidgetElement element)
{

	bool contains = false;
	
	if (element.getItemDescription() == "PhotometricInterpretation" || element.getItemDescription() =="Rows" || element.getItemDescription()== "Columns" || 
		element.getItemDescription().toUpper().contains("PIXEL") || element.getItemDescription().toUpper().contains("FRAME") || element.getItemDescription().toUpper().contains("BIT"))
		contains = true;

	DcmTagKey tagKey = element.extractTagKey();

	if (tagKey.getElement() == 0 || tagKey.getGroup() == 0 || tagKey.getGroup() == 2 || tagKey.getGroup() == 1
		|| tagKey.getGroup() == 5 || tagKey.getGroup() == 3 || tagKey.getGroup() == 7 || tagKey.getGroup() == 0xffff || contains)
		return false;

	return true;
}

//========================================================================================================================
int DICOMViewer::currentRow(DcmWidgetElement element, const int& finalRow)
{

	int index = 0;

	for (int i = 0; i <= finalRow; i++)
	{
		DcmWidgetElement innerElement = DcmWidgetElement(ui.tableWidget->item(i,0)->text(), ui.tableWidget->item(i, 1)->text(), 
			ui.tableWidget->item(i, 2)->text(), ui.tableWidget->item(i, 3)->text(), ui.tableWidget->item(i, 4)->text(), ui.tableWidget->item(i, 5)->text());

		if (element == innerElement)
			index++;
	}

	return index;

}

//========================================================================================================================
void DICOMViewer::deleteClicked()
{
	QList<QTableWidgetItem*> items = ui.tableWidget->selectedItems();
	DcmWidgetElement element = DcmWidgetElement(items[0]->text(), items[1]->text(), items[2]->text(), items[3]->text(), items[4]->text(), items[5]->text());
	element.calculateTableIndex(currentRow(element,ui.tableWidget->currentRow()),this->elements);
	element.calculateDepthFromTag();
	QList<DcmWidgetElement> list;
	list.append(element);
	generatePathToRoot(element, element.getTableIndex(), &list);

	if (list.size() > 1)
	{
		DcmSequenceOfItems* sequence;

		if (file.getDataset()->findAndGetSequence(list[list.size() - 1].extractTagKey(), sequence, false, false).good())
		{
			list.removeLast();

			if (deleteElementFromFile(sequence, element, list))
			{
				clearTable();
				extractData(file);
			}
		}
	}

	else
	{
		if (file.getDataset()->findAndDeleteElement(element.extractTagKey(), false, false).good())
		{
			clearTable();
			extractData(file);
		}
	}
}

//========================================================================================================================
void DICOMViewer::insertClicked()
{
	TagSelectDialog* dialog = new TagSelectDialog(nullptr);
	dialog->populate();

	if (dialog->exec() == QDialog::Accepted)
	{
		DcmWidgetElement insertElement = dialog->getElement();
		QList<QTableWidgetItem*> items = ui.tableWidget->selectedItems();

		if (items.size())
		{
			DcmWidgetElement selectedElement = DcmWidgetElement(items[0]->text(), items[1]->text(), items[2]->text(), items[3]->text(), items[4]->text(), items[5]->text());
			selectedElement.calculateTableIndex(currentRow(selectedElement, ui.tableWidget->currentRow()), this->elements);
			QList<DcmWidgetElement> list;
			list.append(selectedElement);
			generatePathToRoot(selectedElement, selectedElement.getTableIndex(), &list);

			if (selectedElement.getItemVR() == "SQ" && insertElement.getItemVR() == "na")
			{
				DcmSequenceOfItems* sequence;

				if (file.getDataset()->findAndGetSequence(list[list.size() - 1].extractTagKey(), sequence, false, false).good())
				{
					if (list.size() == 1)
					{
						DcmItem* it = new DcmItem(DcmTag(insertElement.extractTagKey()));

						if (!file.getDataset()->insertSequenceItem(sequence->getTag().getBaseTag(), it).good())
						{
							alertFailed("Failed!");
						}

						else
						{
							clearTable();
							extractData(file);
						}
					}

					else
					{
						list.removeLast();

						if (this->insertElement(sequence, selectedElement, insertElement, list))
						{
							clearTable();
							extractData(file);
						}
					}
				}
			}

			else if (selectedElement.getItemVR() == "na")
			{
				DcmSequenceOfItems* sequence;

				if (file.getDataset()->findAndGetSequence(list[list.size() - 1].extractTagKey(), sequence, false, false).good())
				{
					list.removeLast();

					if (this->insertElement(sequence, selectedElement, insertElement, list))
					{
						clearTable();
						extractData(file);
					}
				}
			}

			else if (selectedElement.getItemVR() == "SQ" && insertElement.getItemVR() != "na")
			{
				alertFailed("Can only insert items in sequence!");
			}

			else
			{
				if (!file.getDataset()->putAndInsertString(insertElement.extractTagKey(), insertElement.getItemValue().toStdString().c_str(), false).good())
				{
					alertFailed("Failed!");
				}

				else
				{
					clearTable();
					extractData(file);
				}
			}
		}

		else
		{
			if (insertElement.getItemVR() == "SQ")
			{
				if (!file.getDataset()->insertEmptyElement(insertElement.extractTagKey(), false).good())
				{
					alertFailed("Failed!");
				}
				else
				{
					clearTable();
					extractData(file);
				}
			}
			else if (!file.getDataset()->putAndInsertString(insertElement.extractTagKey(),insertElement.getItemValue().toStdString().c_str(),false).good())
			{
				alertFailed("Failed!");
			}
			else
			{
				clearTable();
				extractData(file);
			}
		}
	}

	delete dialog;
}