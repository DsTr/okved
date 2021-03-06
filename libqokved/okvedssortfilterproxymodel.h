#ifndef OKVEDSSORTFILTERPROXYMODEL_H
#define OKVEDSSORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QHash>
#include <QtSql>

enum filter_dest { NUMBER, NAME, NONE };

typedef QHash<QString, int> CheckedList;
Q_DECLARE_METATYPE(CheckedList)

class OkvedsSortFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
private:
    bool hide_not_checked;
    QString okved_table_name;   //чтобы записывать настройки с галками

public:
    void setFilter(QString value);
    QString filter_string;
    int filter_type;
    void setHideChecks(bool value) { hide_not_checked = value; invalidateFilter();}
    bool getHideChecks() { return hide_not_checked; }
    explicit OkvedsSortFilterProxyModel(QObject *parent = 0);
    ~OkvedsSortFilterProxyModel();
    CheckedList user_check;
    CheckedList check;
    inline CheckedList getUserCheckList(){ return user_check; }
    QVariant data(const QModelIndex &item, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );
    void setSourceModel ( QSqlTableModel * sourceModel );
    void setGlobalChecksList ( CheckedList checklist  );
    void setUserChecksList ( );

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;


signals:

public slots:

};

#endif // OKVEDSSORTFILTERPROXYMODEL_H
