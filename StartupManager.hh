#ifndef STARTUPMANAGER_H
#define STARTUPMANAGER_H

#include <QMainWindow>
#include <QListView>
#include <QStandardItemModel>
#include <QDir>

class StartupManager : public QMainWindow {
    Q_OBJECT
public:
    StartupManager(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QDir startupDir;
    QListView* listView;
    QStandardItemModel* model;
    const int FilePathRole = Qt::UserRole + 1;

    void addToStartup(const QString& filePath);
    void handleShortcut(const QString& shortcutPath);
    QString extractValue(const QString& content, const QString& key);
    void loadStartupItems();
    void showContextMenu(const QPoint& pos);
    void showExplorerContextMenu(const QString& filePath, const QPoint& globalPos);
};

#endif // STARTUPMANAGER_H
