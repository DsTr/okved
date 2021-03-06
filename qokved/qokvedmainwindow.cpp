#include "qokvedmainwindow.h"
#include "ui_qokvedmainwindow.h"

#include "addokveddialog.h"
#include "listsmanipulations.h"

#include <QDebug>
#include <QFile>
#include <QClipboard>
#include <QTextTable>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QMap>

QOkvedMainWindow::QOkvedMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::QOkvedMainWindow)
{
    ui->setupUi(this);
    ui->additionView->installEventFilter(this);

    qokved = new Libqokved(this);

    okveds_proxy_model = new OkvedsSortFilterProxyModel(this);
    ui->okvedsView->setModel(okveds_proxy_model);

    #ifdef Q_WS_WIN
        appDir = QDir::convertSeparators(QDir::homePath() + "/Application Data/qokved");
    #elif defined(Q_WS_X11)
        appDir = QDir::homePath() + "/.config/qokved/";
    #endif

    QDir::root().mkpath(appDir);

    QString db_path=QDir::convertSeparators(appDir + "qokved.db");
    if (!QFile(db_path).exists())
    {
        QString def_db_path = findExistPath(QStringList() << QDir::convertSeparators(QCoreApplication::applicationDirPath () + "/../share/qokved/templates/qokved.db.default")  << QDir::convertSeparators(QCoreApplication::applicationDirPath() + "/templates/qokved.db.default") );

       if (!def_db_path.isNull())
        {
            QFile(def_db_path).copy(db_path);
        } else errorMessage(QString::fromUtf8("Не найдена база данных, будет создана новая!"));
    }

    qokved->setDbPath(db_path);

    QSettings settings("qokved", "qokved");
    ui->filterEdit->setText(settings.value("last_filter").toString());

    updateVersionsList();
    razdels_update();

    row_filter_update();


    ods_writer = new OdsWriter(this);
    connect(ods_writer, SIGNAL(errorMessage(QString)), this, SLOT(errorMessage(QString)));
}

void QOkvedMainWindow::action_create_base_list()
{
    bool ok;

    QString name = QInputDialog::getItem ( this, QString::fromUtf8("Введите название нового списка, либо укажите из списка старый для замены"), QString::fromUtf8("Предопределенный список:"), qokved->globalLists(), 0, true, &ok );

    if(ok && !name.isEmpty())
	qokved->save_global_list(okveds_proxy_model->getUserCheckList(), name);
}

void QOkvedMainWindow::action_remove_base_list()
{
    if (qokved->globalLists().count() == 0)
    {
        errorMessage(QString::fromUtf8("Нет ни одного списка для удаления"));
        return;
    }
    bool ok;

    QString name = QInputDialog::getItem ( this, QString::fromUtf8("Список для удаления"), QString::fromUtf8("Предопределенный список:"), qokved->globalLists(), 0, false, &ok );

    if(ok && !name.isEmpty())
        qokved->removeGlobalList(name);
}

void QOkvedMainWindow::selectList()
{
    bool ok;
    QStringList checksList;
    checksList.append(QString::fromUtf8("Пользовательский"));
    checksList.append(qokved->globalLists());

    QString name = QInputDialog::getItem( this, QString::fromUtf8("Введите название нового списка, либо укажите из списка старый для замены"), QString::fromUtf8("Название предопределенного списка:"), checksList, 0, false, &ok );

    if(ok && !name.isEmpty())
	if(name == QString::fromUtf8("Пользовательский"))
	{
	    okveds_proxy_model->setHideChecks(false);
	    ui->checkedButton->setChecked(false);
	    ui->checkedButton->setEnabled(true);
	    okveds_proxy_model->setUserChecksList();
	} else {
	    okveds_proxy_model->setHideChecks(true);
	    ui->checkedButton->setChecked(true);
	    ui->checkedButton->setEnabled(false);
	    okveds_proxy_model->setGlobalChecksList(qokved->getGlobalList(name));
	}

}

void QOkvedMainWindow::versionsIndexChanged( int index )
{
    if (index != -1)
	qokved->setActiveVersion(ui->okvedVersionBox->itemData(index).toInt());
    razdels_update();
}

void QOkvedMainWindow::actionBlockDbEdit(bool block)
{
    db_edit_blocked = block;

    ui->additionView->setReadOnly(db_edit_blocked);
    ui->action_create_db_from_txt->setEnabled(!db_edit_blocked);
    ui->action_create_base_list->setEnabled(!db_edit_blocked);

    if (db_edit_blocked)
    {
	ui->razdelsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->okvedsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    } else {
	ui->razdelsView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
	ui->okvedsView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    }
}

void QOkvedMainWindow::checkButtonClicked ( bool checked )
{
    okveds_proxy_model->setHideChecks(checked);
}

void QOkvedMainWindow::updateVersionsList()
{
    QMapIterator<int, QString> i(qokved->versions());
    QString row;
    while (i.hasNext()) {
	i.next();
	ui->okvedVersionBox->addItem(i.value(), i.key());
    }

    QSettings settings("qokved", "qokved");
    int cur_ind = ui->okvedVersionBox->findData(settings.value("last_version", 1).toInt());
    if (cur_ind > -1)
    {
        ui->okvedVersionBox->setCurrentIndex(cur_ind);
    } else ui->okvedVersionBox->setCurrentIndex( ui->okvedVersionBox->count()-1 );
}

void QOkvedMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "event";
    event->accept();
}

void QOkvedMainWindow::createDbFromTxt()
{
    bool ok;
    QString text = QInputDialog::getText(this, QString::fromUtf8("Введите название нового закона"),
                                         QString::fromUtf8("Закон: "), QLineEdit::Normal,
					 "", &ok);
    if (ok && !text.isEmpty())
    {

        QString fileName = QFileDialog::getOpenFileName(this,
                                                QString::fromUtf8("Укажите путь к закону в txt"), QDir::homePath(), "Text Files (*.txt)");

        int ver_id = qokved->createVersion(text);
	ui->okvedVersionBox->addItem(text, ver_id);
	ui->okvedVersionBox->setCurrentIndex(ui->okvedVersionBox->count()-1);
	//qokved->setActiveVersion(ver_id);

        if (!fileName.isEmpty())
        {
            QFile file(fileName);
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QString data = QString::fromUtf8(file.readAll().data());
            qokved->fill_db_from_zakon(data);
            razdels_update();
        }
    }
}

void QOkvedMainWindow::errorMessage(QString message)
{
    QMessageBox::critical(this, "QOkved", message, QMessageBox::Ok);
}

QString QOkvedMainWindow::findExistPath(QStringList path_list)
{
    for (int i = 0; i < path_list.size(); ++i)
    {
        if ( QFile(path_list.at(i)).exists() ) return path_list.at(i);
    }
    return QString::null;
}

void QOkvedMainWindow::action_oocalc()
{
    if (ui->okvedsView->model()->rowCount()==0) return;

    QString template_path = findExistPath(QStringList() << QDir::convertSeparators(QCoreApplication::applicationDirPath () + "/templates/soffice.ods") << QDir::convertSeparators(QCoreApplication::applicationDirPath () + "/../share/qokved/templates/soffice.ods"));
    if (template_path.isEmpty() || template_path.isNull())
    {
        errorMessage(QString::fromUtf8("Невозможно найти путь к soffice.ods"));
        return;
    }
    ods_writer->open(template_path);

    QMap<QString, QString> table;

    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());
    for(int i = 0; i < model->rowCount(); i++)
    {
        if (!ui->okvedsView->isRowHidden(i))
            table.insert(model->data(model->index(i, 1)).toString(), model->data(model->index(i, 2)).toString());
    }

    ods_writer->writeTable(table);
    QString ods_temp = QString(QDir::tempPath()+"/%1").arg(QDateTime::currentDateTime().toTime_t()) + ".ods";
    ods_writer->save(ods_temp);
    ods_writer->startOO(ods_temp);
}

void QOkvedMainWindow::row_filter_update()
{
     QString filter = ui->filterEdit->text();
     
     okveds_proxy_model->setFilter(filter);
}

void QOkvedMainWindow::razdels_update()
{
    ui->razdelsView->setModel(qokved->razdels_model());
    ui->razdelsView->hideColumn(0);

    ui->okvedsView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);

  //  ui->okvedsView->horizontalHeader()->setResizeMode(2,QHeaderView::Stretch);

    QHeaderView* hw = ui->razdelsView->horizontalHeader();
    hw->setResizeMode(QHeaderView::Stretch );

    //currentRowChanged ( const QModelIndex & current, const QModelIndex & previous )
    connect(ui->razdelsView->selectionModel(),SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex & )),this,SLOT(razdels_row_changed()));
    ui->razdelsView->hideColumn(2);
    ui->razdelsView->hideColumn(3);

    ui->razdelsView->selectRow(0);

   // row_filter_update();
}

void QOkvedMainWindow::razdels_row_changed()
{
    int row = ui->razdelsView->selectionModel()->currentIndex().row();
    if (row > -1){
        QAbstractItemModel *model = ui->razdelsView->model();

        okveds_proxy_model->setSourceModel(qokved->okveds_model(model->data(model->index(row, 0)).toInt()));

        ui->okvedsView->hideColumn(0);
        ui->okvedsView->hideColumn(3);
        ui->okvedsView->hideColumn(4);

        ui->okvedsView->horizontalHeader()->setResizeMode(1,QHeaderView::Interactive);
    //    ui->okvedsView->horizontalHeader()->setResizeMode(1,QHeaderView::Stretch);
       //ui->okvedsView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);

        connect(ui->okvedsView->selectionModel(),SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex & )),this,SLOT(additionUpdate( )));
    }


    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());

    connect ( model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(okvedDataChanged()));

   // row_filter_update();
}

void QOkvedMainWindow::okvedDataChanged()
{
    row_filter_update();
}

void QOkvedMainWindow::okvedUpdate(int row,QSqlRecord& record)
{

}

void QOkvedMainWindow::additionUpdate()
{
    QAbstractItemModel *model = ui->okvedsView->model();
    int row = ui->okvedsView->selectionModel()->currentIndex().row();
    if (row > -1) {
        //model->data()
      QString text = model->data(model->index(row, 3)).toString();
      if (text.isEmpty()) text = QString::fromUtf8("Без описания");
      ui->additionView->setPlainText (text);
    }
}

bool QOkvedMainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusOut)
    {
        if (object == ui->additionView)
        {
            QString text = ui->additionView->toPlainText();
            if (text != m_addition_text) {

                QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());
                QModelIndex sel_mod = ui->okvedsView->selectionModel()->currentIndex();
                int row = sel_mod.row();

                model->setData(model->index(row, 3), text);
                ui->okvedsView->selectRow(row);
            }
        }
    }
    if (event->type() == QEvent::FocusIn)
    {
        if (object == ui->additionView)
        {
           m_addition_text = ui->additionView->toPlainText();

        }
    }
    return false;
}

void QOkvedMainWindow::action_copy_text()
{
    if (ui->okvedsView->model()->rowCount()==0) return;
    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());
    QString buffer;
    for(int i = 0; i < model->rowCount(); i++)
    {
        if (!ui->okvedsView->isRowHidden(i))
            buffer.append(model->data(model->index(i, 1)).toString() + " " + model->data(model->index(i, 2)).toString() + "\n");
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(buffer);
}

void QOkvedMainWindow::action_copy_table()
{
    if (ui->okvedsView->model()->rowCount()==0) return;
    QClipboard *clipboard = QApplication::clipboard();
    QString buffer;
    QMimeData *mime = new QMimeData();
    buffer.append("<table  cellpadding=\"4\" cellspacing=\"0\">");

    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());
    buffer.append(QString::fromUtf8("<tr><td><b>Код по ОКВЭД</b></td><td>Наименование</td></tr>"));
    for(int i = 0; i < model->rowCount(); i++)
    {
        if (!ui->okvedsView->isRowHidden(i))
            buffer.append("<tr><td>"+model->data(model->index(i, 1)).toString()+"</td>" + " " + "<td WIDTH=400>"+model->data(model->index(i, 2)).toString()+"</td></tr>");
    }
    buffer.append("</table>");

    mime->setHtml(buffer);
    clipboard->setMimeData(mime);

}

//Сдвигает позиции начиная прибавляет к rid ко всем позициям, начиная с row
void QOkvedMainWindow::freeRid(int row)
{
    QSqlTableModel *model = static_cast<QSqlTableModel*>(ui->razdelsView->model());
    for(int i = model->rowCount(); i >= row; i--)  //Сдвиг всех разделов для вставки подраздела
    {
        QSqlRecord rec = model->record(i);
        int rid = rec.value("rid").toInt();
        rec.setValue("rid", rid+1);
        model->setRecord(i, rec);
    }
}

void QOkvedMainWindow::razdelzTablePopup(const QPoint & pos)
{
    QMenu myMenu;
    QPoint globalPos = ui->razdelsView->mapToGlobal(pos);
    QModelIndex sel_mod = ui->razdelsView->selectionModel()->currentIndex();
    int row = sel_mod.row();
    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->razdelsView->model());
    if (!db_edit_blocked)
    {
	if (model->data(model->index(row, 3)).toString() == "0" )
	    myMenu.addAction(QString::fromUtf8("Создать подраздел"));

	myMenu.addAction(QString::fromUtf8("Создать раздел"));
	myMenu.addAction(QString::fromUtf8("Удалить раздел"));
    }

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        if (selectedItem->text() == QString::fromUtf8("Удалить раздел"))
        {
            if (row > -1)
            {
                //удаление всех оквэдов, относящихся к разделу
                if (row == 0) return; //Если "все разделы"
                QAbstractItemModel *model = ui->razdelsView->model();
                QSqlTableModel *model_okved = qokved->okveds_model(model->data(model->index(row, 0)).toInt());
                model_okved->removeRows(0, model_okved->rowCount());
                ui->razdelsView->selectRow(row-1);
                model->removeRow(row);//Удаление самого раздела

            }
        }
        if (selectedItem->text() == QString::fromUtf8("Создать раздел") || selectedItem->text() == QString::fromUtf8("Создать подраздел"))
        {
            bool ok;
            QString text = QInputDialog::getText(this, QString::fromUtf8("Создать (под)раздел"),
                                                 QString::fromUtf8("Название нового (под)раздела:"), QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty())
            {
                QSqlRecord record;
                int dist_row = -1;
                QSqlField field("name", QVariant::String);
                field.setValue(text);
                record.append(field);
                QSqlField fieldf("father", QVariant::Int);
                fieldf.setValue(0);
                if (selectedItem->text() == QString::fromUtf8("Создать подраздел"))
                {
                    QSqlTableModel *model = static_cast<QSqlTableModel*>(ui->razdelsView->model());
                    freeRid(row+1);
                    QSqlField rid_field("rid", QVariant::Int);
                    rid_field.setValue(model->record(row).value("rid").toInt()+1);
                    record.append(rid_field);

                    fieldf.setValue(model->data(model->index(row, 0)).toInt());
                    dist_row=row+1;
                }

                record.append(fieldf);
                model->insertRecord(dist_row, record);
                int index_row=model->rowCount()-1;
                if (dist_row!=-1) index_row = dist_row;
                ui->razdelsView->selectRow(index_row);
            }
        }

    }
}

void QOkvedMainWindow::tablePopup(const QPoint & pos)
{
    QPoint globalPos = ui->okvedsView->mapToGlobal(pos);
    QModelIndex sel_mod = ui->okvedsView->selectionModel()->currentIndex();
    int row = sel_mod.row();

    QMenu myMenu;
    if (!db_edit_blocked)
    {
    myMenu.addAction(QString::fromUtf8("Добавить ОКВЭД"));
    myMenu.addAction(QString::fromUtf8("Удалить ОКВЭД"));
    }
    // ...

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        if (selectedItem->text() == QString::fromUtf8("Удалить ОКВЭД"))
        {
            if (row > -1)
            {

                ui->okvedsView->model()->removeRow(row);
                int new_row;
                if (row == 0) {
                    new_row = 1;
                } else {
                    new_row = row -1;
                }
                ui->okvedsView->selectRow(new_row);
             }
        }
        if (selectedItem->text() == QString::fromUtf8("Добавить ОКВЭД"))
        {
                int row = ui->razdelsView->selectionModel()->currentIndex().row();
                AddOkvedDialog *dialog = new AddOkvedDialog(this);
                QSqlTableModel *model = static_cast<QSqlTableModel*>(ui->razdelsView->model());
                for(int i = 1; i < model->rowCount()-1; i++)
                {
                    dialog->addRazdel(model->data(model->index(i, 1)).toString(), model->data(model->index(i, 0)).toString());
                }
                if (row > 0)
                    dialog->setActiveRazdel(model->data(model->index(row, 0)).toString());
                connect(dialog, SIGNAL(addNewOkved(QString, QString, QString, QString)), this, SLOT(addNewOkved(QString, QString, QString, QString)));
                dialog->exec();
        }
    }
}


void QOkvedMainWindow::addNewOkved(QString rid, QString number, QString name, QString caption)
{
    QSqlTableModel *model =  static_cast<QSqlTableModel*>(ui->okvedsView->model());

    QSqlRecord record;
    QSqlField field_name("name", QVariant::String);
    field_name.setValue(name);
    record.append(field_name);

    QSqlField field_num("number", QVariant::String);
    field_num.setValue(number);
    record.append(field_num);

    QSqlField field_cap("addition", QVariant::String);
    field_cap.setValue(caption);
    record.append(field_cap);

    QSqlField field_rid("razdel_id", QVariant::Int);
    field_rid.setValue(rid.toInt());
    record.append(field_rid);

    model->insertRecord(-1, record);

}

QOkvedMainWindow::~QOkvedMainWindow()
{
    QSettings settings("qokved", "qokved");
    settings.setValue("last_filter", ui->filterEdit->text());
    settings.setValue("last_version", ui->okvedVersionBox->itemData(ui->okvedVersionBox->currentIndex()).toInt());

    delete ui;
    delete qokved;
    delete ods_writer;
}
