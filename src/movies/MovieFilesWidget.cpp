#include "movies/MovieFilesWidget.h"
#include "ui_MovieFilesWidget.h"

#include "data/Movie.h"
#include "data/MovieModel.h"
#include "data/MovieProxyModel.h"
#include "globals/Globals.h"
#include "globals/Helper.h"
#include "globals/LocaleStringCompare.h"
#include "globals/Manager.h"
#include "movies/MovieMultiScrapeDialog.h"
#include "ui/small_widgets/AlphabeticalList.h"
#include "ui/small_widgets/LoadingStreamDetails.h"

#include <QDesktopServices>
#include <QLocale>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTableWidget>
#include <QTimer>

MovieFilesWidget* MovieFilesWidget::m_instance;

/**
 * @brief MovieFilesWidget::FilesWidget
 * @param parent
 */
MovieFilesWidget::MovieFilesWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MovieFilesWidget)
{
    m_instance = this;
    ui->setupUi(this);
    ui->statusLabel->setText(tr("%n movies", "", 0));

#ifdef Q_OS_WIN32
    ui->verticalLayout->setContentsMargins(0, 0, 0, 1);
#endif

    m_lastMovie = nullptr;
    m_mouseIsIn = false;
    m_movieProxyModel = new MovieProxyModel(this);
    m_movieProxyModel->setSourceModel(Manager::instance()->movieModel());
    m_movieProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_movieProxyModel->setDynamicSortFilter(true);
    ui->files->setModel(m_movieProxyModel);
    for (int i = 1, n = ui->files->model()->columnCount(); i < n; ++i) {
        ui->files->setColumnWidth(i, 24);
        ui->files->setColumnHidden(i, true);
    }
    ui->files->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
#ifdef Q_OS_WIN
    ui->files->setIconSize(QSize(12, 12));
#else
    ui->files->setIconSize(QSize(16, 16));
#endif

    for (const MediaStatusColumn& column : Settings::instance()->mediaStatusColumns()) {
        ui->files->setColumnHidden(MovieModel::mediaStatusToColumn(column), false);
    }

    m_alphaList = new AlphabeticalList(this, ui->files);

    QMenu* mediaStatusColumnsMenu = new QMenu(tr("Media Status Columns"), ui->files);
    for (int i = static_cast<int>(MediaStatusColumn::First), n = static_cast<int>(MediaStatusColumn::Last); i <= n;
         ++i) {
        QAction* action = new QAction(MovieModel::mediaStatusToText(static_cast<MediaStatusColumn>(i)), this);
        action->setProperty("mediaStatusColumn", i);
        action->setCheckable(true);
        action->setChecked(Settings::instance()->mediaStatusColumns().contains(static_cast<MediaStatusColumn>(i)));
        connect(action, &QAction::triggered, this, &MovieFilesWidget::onActionMediaStatusColumn);
        mediaStatusColumnsMenu->addAction(action);
    }

    QMenu* labelsMenu = new QMenu(tr("Label"), ui->files);
    QMapIterator<ColorLabel, QString> it(Helper::instance()->labels());
    while (it.hasNext()) {
        it.next();
        auto action = new QAction(it.value(), this);
        action->setIcon(Helper::instance()->iconForLabel(it.key()));
        action->setProperty("color", static_cast<int>(it.key()));
        connect(action, &QAction::triggered, this, &MovieFilesWidget::onLabel);
        labelsMenu->addAction(action);
    }

    QAction* actionMultiScrape = new QAction(tr("Load Information"), this);
    QAction* actionMarkAsWatched = new QAction(tr("Mark as watched"), this);
    QAction* actionMarkAsUnwatched = new QAction(tr("Mark as unwatched"), this);
    QAction* actionLoadStreamDetails = new QAction(tr("Load Stream Details"), this);
    QAction* actionMarkForSync = new QAction(tr("Add to Synchronization Queue"), this);
    QAction* actionUnmarkForSync = new QAction(tr("Remove from Synchronization Queue"), this);
    QAction* actionOpenFolder = new QAction(tr("Open Movie Folder"), this);
    QAction* actionOpenNfo = new QAction(tr("Open NFO File"), this);

    m_contextMenu = new QMenu(ui->files);
    m_contextMenu->addAction(actionMultiScrape);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(actionMarkAsWatched);
    m_contextMenu->addAction(actionMarkAsUnwatched);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(actionLoadStreamDetails);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(actionMarkForSync);
    m_contextMenu->addAction(actionUnmarkForSync);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(actionOpenFolder);
    m_contextMenu->addAction(actionOpenNfo);
    m_contextMenu->addSeparator();
    m_contextMenu->addMenu(labelsMenu);
    m_contextMenu->addMenu(mediaStatusColumnsMenu);

    // clang-format off
    connect(actionMultiScrape,       &QAction::triggered, this, &MovieFilesWidget::multiScrape);
    connect(actionMarkAsWatched,     &QAction::triggered, this, &MovieFilesWidget::markAsWatched);
    connect(actionMarkAsUnwatched,   &QAction::triggered, this, &MovieFilesWidget::markAsUnwatched);
    connect(actionLoadStreamDetails, &QAction::triggered, this, &MovieFilesWidget::loadStreamDetails);
    connect(actionMarkForSync,       &QAction::triggered, this, &MovieFilesWidget::markForSync);
    connect(actionUnmarkForSync,     &QAction::triggered, this, &MovieFilesWidget::unmarkForSync);
    connect(actionOpenFolder,        &QAction::triggered, this, &MovieFilesWidget::openFolder);
    connect(actionOpenNfo,           &QAction::triggered, this, &MovieFilesWidget::openNfoFile);

    connect(ui->files,                   &QWidget::customContextMenuRequested, this, &MovieFilesWidget::showContextMenu);
    connect(ui->files->selectionModel(), &QItemSelectionModel::currentChanged, this, &MovieFilesWidget::itemActivated);
    connect(ui->files->model(),          &QAbstractItemModel::dataChanged,     this, &MovieFilesWidget::setAlphaListData);
    connect(ui->files,                   &MyTableView::sigLeftEdge,            this, &MovieFilesWidget::onLeftEdge);
    connect(ui->files,                   &QAbstractItemView::doubleClicked,    this, &MovieFilesWidget::playMovie);

    connect(m_alphaList, &AlphabeticalList::sigAlphaClicked, this, &MovieFilesWidget::scrollToAlpha);

    connect(ui->sortByNew,       &MyLabel::clicked, this, &MovieFilesWidget::onSortByNew);
    connect(ui->sortByName,      &MyLabel::clicked, this, &MovieFilesWidget::onSortByName);
    connect(ui->sortByLastAdded, &MyLabel::clicked, this, &MovieFilesWidget::onSortByAdded);
    connect(ui->sortBySeen,      &MyLabel::clicked, this, &MovieFilesWidget::onSortBySeen);
    connect(ui->sortByYear,      &MyLabel::clicked, this, &MovieFilesWidget::onSortByYear);

    connect(m_movieProxyModel, &QAbstractItemModel::rowsInserted, this, &MovieFilesWidget::onViewUpdated);
    connect(m_movieProxyModel, &QAbstractItemModel::rowsRemoved,  this, &MovieFilesWidget::onViewUpdated);
    // clang-format on
}

/**
 * @brief MovieFilesWidget::~FilesWidget
 */
MovieFilesWidget::~MovieFilesWidget()
{
    delete ui;
}

/**
 * @brief Returns the current instance
 * @return Instance of FilesWidget
 */
MovieFilesWidget* MovieFilesWidget::instance()
{
    return m_instance;
}

void MovieFilesWidget::resizeEvent(QResizeEvent* event)
{
    int scrollBarWidth = 0;
    if (ui->files->verticalScrollBar()->isVisible()) {
        scrollBarWidth = ui->files->verticalScrollBar()->width();
    }
    m_alphaList->setRightSpace(scrollBarWidth + 5);
    m_alphaList->setBottomSpace(ui->sortLabelWidget->height() + 10);
    m_alphaList->adjustSize();
    QWidget::resizeEvent(event);
}

void MovieFilesWidget::showContextMenu(QPoint point)
{
    m_contextMenu->exec(ui->files->mapToGlobal(point));
}

void MovieFilesWidget::multiScrape()
{
    m_contextMenu->close();
    QVector<Movie*> movies = selectedMovies();
    if (movies.isEmpty()) {
        return;
    }

    if (movies.count() == 1) {
        emit sigStartSearch();
        return;
    }

    MovieMultiScrapeDialog::instance()->setMovies(movies);
    int result = MovieMultiScrapeDialog::instance()->exec();
    if (result == QDialog::Accepted) {
        movieSelectedEmitter();
    }
}

void MovieFilesWidget::markAsWatched()
{
    m_contextMenu->close();

    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        const int row = index.model()->data(index, Qt::UserRole).toInt();
        Movie* movie = Manager::instance()->movieModel()->movie(row);
        movie->setWatched(true);
        if (movie->playcount() < 1) {
            movie->setPlayCount(1);
        }
        if (!movie->lastPlayed().isValid()) {
            movie->setLastPlayed(QDateTime::currentDateTime());
        }
    }
    if (ui->files->selectionModel()->selectedRows(0).count() > 0) {
        movieSelectedEmitter();
    }
}

void MovieFilesWidget::markAsUnwatched()
{
    m_contextMenu->close();

    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        const int row = index.model()->data(index, Qt::UserRole).toInt();
        Movie* movie = Manager::instance()->movieModel()->movie(row);
        if (movie->watched()) {
            movie->setWatched(false);
        }
        if (movie->playcount() != 0) {
            movie->setPlayCount(0);
        }
    }
    if (ui->files->selectionModel()->selectedRows(0).count() > 0) {
        movieSelectedEmitter();
    }
}

void MovieFilesWidget::loadStreamDetails()
{
    m_contextMenu->close();
    QVector<Movie*> movies;
    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        int row = index.model()->data(index, Qt::UserRole).toInt();
        Movie* movie = Manager::instance()->movieModel()->movie(row);
        movies.append(movie);
    }
    if (movies.count() == 1) {
        movies.at(0)->controller()->loadStreamDetailsFromFile();
        movies.at(0)->setChanged(true);
    } else {
        auto loader = new LoadingStreamDetails(this);
        loader->loadMovies(movies);
        delete loader;
    }
    movieSelectedEmitter();
    m_movieProxyModel->setSourceModel(Manager::instance()->movieModel());
}

void MovieFilesWidget::markForSync()
{
    m_contextMenu->close();
    QVector<QModelIndex> indexes;
    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        indexes << index;
    }
    for (const QModelIndex& index : indexes) {
        int row = index.model()->data(index, Qt::UserRole).toInt();
        Movie* movie = Manager::instance()->movieModel()->movie(row);
        movie->setSyncNeeded(true);
        ui->files->update(index);
    }
}

void MovieFilesWidget::unmarkForSync()
{
    m_contextMenu->close();
    QVector<QModelIndex> indexes;
    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        indexes << index;
    }
    for (const QModelIndex& index : indexes) {
        int row = index.model()->data(index, Qt::UserRole).toInt();
        Movie* movie = Manager::instance()->movieModel()->movie(row);
        movie->setSyncNeeded(false);
        ui->files->update(index);
    }
}

void MovieFilesWidget::openFolder()
{
    m_contextMenu->close();
    if (!ui->files->currentIndex().isValid()) {
        return;
    }
    int row = ui->files->currentIndex().data(Qt::UserRole).toInt();
    Movie* movie = Manager::instance()->movieModel()->movie(row);
    if (!movie || movie->files().isEmpty()) {
        return;
    }
    QFileInfo fi(movie->files().at(0));
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void MovieFilesWidget::openNfoFile()
{
    m_contextMenu->close();
    if (!ui->files->currentIndex().isValid()) {
        return;
    }
    int row = ui->files->currentIndex().data(Qt::UserRole).toInt();
    Movie* movie = Manager::instance()->movieModel()->movie(row);
    if (!movie || movie->files().isEmpty()) {
        return;
    }

    QFileInfo fi(Manager::instance()->mediaCenterInterface()->nfoFilePath(movie));
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
}

/**
 * @brief Called when an item has selected
 * @param index
 * @param previous
 */
void MovieFilesWidget::itemActivated(QModelIndex index, QModelIndex previous)
{
    qDebug() << "Entered";
    if (!index.isValid()) {
        qDebug() << "Index is invalid";
        m_lastMovie = nullptr;
        emit noMovieSelected();
        return;
    }
    m_lastModelIndex = previous;
    int row = index.model()->data(index, Qt::UserRole).toInt();
    m_lastMovie = Manager::instance()->movieModel()->movie(row);
    QTimer::singleShot(0, this, &MovieFilesWidget::movieSelectedEmitter);
}

/**
 * @brief Just emits movieSelected
 */
void MovieFilesWidget::movieSelectedEmitter()
{
    qDebug() << "Entered";
    if (m_lastMovie) {
        emit movieSelected(m_lastMovie);
    }
}

/**
 * @brief Sets the filters
 * @param filters List of filters
 * @param text Filter text
 */
void MovieFilesWidget::setFilter(QVector<Filter*> filters, QString text)
{
    m_movieProxyModel->setFilter(filters, text);
    m_movieProxyModel->setFilterWildcard("*" + text + "*");
    setAlphaListData();
    onViewUpdated();
}

/**
 * @brief Restores the last selected item
 */
void MovieFilesWidget::restoreLastSelection()
{
    qDebug() << "Entered";
    ui->files->setCurrentIndex(m_lastModelIndex);
}


void MovieFilesWidget::updateSort(SortBy sortBy)
{
    ui->sortByNew->setProperty("active", sortBy == SortBy::New);
    ui->sortByLastAdded->setProperty("active", sortBy == SortBy::Added);
    ui->sortByName->setProperty("active", sortBy == SortBy::Name);
    ui->sortByYear->setProperty("active", sortBy == SortBy::Year);
    ui->sortBySeen->setProperty("active", sortBy == SortBy::Seen);

    // We have to rerender the labels because of the dynamic property "active".
    style()->unpolish(ui->sortByNew);
    style()->unpolish(ui->sortByName);
    style()->unpolish(ui->sortByLastAdded);
    style()->unpolish(ui->sortBySeen);
    style()->unpolish(ui->sortByYear);
    style()->polish(ui->sortByNew);
    style()->polish(ui->sortByName);
    style()->polish(ui->sortByLastAdded);
    style()->polish(ui->sortBySeen);
    style()->polish(ui->sortByYear);

    m_movieProxyModel->setSortBy(sortBy);
}

/**
 * @brief Adjusts labels and sets sort by to added
 */
void MovieFilesWidget::onSortByAdded()
{
    updateSort(SortBy::Added);
}

/**
 * @brief Adjusts labels and sets sort by to name
 */
void MovieFilesWidget::onSortByName()
{
    updateSort(SortBy::Name);
}

/**
 * @brief Adjusts labels and sets sort by to name
 */
void MovieFilesWidget::onSortByNew()
{
    updateSort(SortBy::New);
}

/**
 * @brief Adjusts labels and sets sort by to seen
 */
void MovieFilesWidget::onSortBySeen()
{
    updateSort(SortBy::Seen);
}

/**
 * @brief Adjusts labels and sets sort by to year
 */
void MovieFilesWidget::onSortByYear()
{
    updateSort(SortBy::Year);
}

QVector<Movie*> MovieFilesWidget::selectedMovies()
{
    QVector<Movie*> movies;
    for (const QModelIndex& index : ui->files->selectionModel()->selectedRows(0)) {
        int row = index.model()->data(index, Qt::UserRole).toInt();
        movies.append(Manager::instance()->movieModel()->movie(row));
    }
    if (movies.isEmpty()) {
        movies << m_lastMovie;
    }
    return movies;
}

void MovieFilesWidget::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_mouseIsIn = true;
}

void MovieFilesWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_mouseIsIn = false;
    m_alphaList->hide();
}

void MovieFilesWidget::setAlphaListData()
{
    QStringList alphas;
    for (int i = 0, n = ui->files->model()->rowCount(); i < n; ++i) {
        QString title = ui->files->model()->data(ui->files->model()->index(i, 0)).toString();
        QString first = title.left(1).toUpper();
        if (!alphas.contains(first)) {
            alphas.append(first);
        }
    }
    qSort(alphas.begin(), alphas.end(), LocaleStringCompare());
    int scrollBarWidth = 0;
    if (ui->files->verticalScrollBar()->isVisible()) {
        scrollBarWidth = ui->files->verticalScrollBar()->width();
    }
    m_alphaList->setRightSpace(scrollBarWidth + 5);
    m_alphaList->setAlphas(alphas);
}

void MovieFilesWidget::scrollToAlpha(QString alpha)
{
    for (int i = 0, n = ui->files->model()->rowCount(); i < n; ++i) {
        QModelIndex index = ui->files->model()->index(i, 0);
        QString title = ui->files->model()->data(index).toString();
        QString first = title.left(1).toUpper();
        if (first == alpha) {
            ui->files->scrollTo(index, QAbstractItemView::PositionAtTop);
            return;
        }
    }
}

void MovieFilesWidget::renewModel()
{
    m_movieProxyModel->setSourceModel(Manager::instance()->movieModel());
    for (int i = 1, n = ui->files->model()->columnCount(); i < n; ++i) {
        ui->files->setColumnHidden(i, true);
    }
    for (const MediaStatusColumn& column : Settings::instance()->mediaStatusColumns()) {
        ui->files->setColumnHidden(MovieModel::mediaStatusToColumn(column), false);
    }
}

void MovieFilesWidget::onLeftEdge(bool isEdge)
{
    if (isEdge && m_mouseIsIn) {
        m_alphaList->show();
    } else {
        m_alphaList->hide();
    }
}

void MovieFilesWidget::selectMovie(Movie* movie)
{
    int row = Manager::instance()->movieModel()->movies().indexOf(movie);
    QModelIndex index = Manager::instance()->movieModel()->index(row, 0, QModelIndex());
    ui->files->selectRow(m_movieProxyModel->mapFromSource(index).row());
}

void MovieFilesWidget::onActionMediaStatusColumn()
{
    m_contextMenu->close();
    auto action = static_cast<QAction*>(QObject::sender());
    if (!action) {
        return;
    }
    action->setChecked(action->isChecked());

    MediaStatusColumn col = static_cast<MediaStatusColumn>(action->property("mediaStatusColumn").toInt());
    QVector<MediaStatusColumn> columns = Settings::instance()->mediaStatusColumns();
    if (action->isChecked() && !columns.contains(col)) {
        columns.append(col);
    } else {
        columns.removeAll(col);
    }
    Settings::instance()->setMediaStatusColumn(columns);
    Settings::instance()->saveSettings();
    renewModel();
}

void MovieFilesWidget::onLabel()
{
    m_contextMenu->close();
    auto action = static_cast<QAction*>(QObject::sender());
    if (!action) {
        return;
    }

    ColorLabel color = static_cast<ColorLabel>(action->property("color").toInt());
    for (Movie* movie : selectedMovies()) {
        movie->setLabel(color);
        Manager::instance()->database()->setLabel(movie->files(), color);
    }
}

void MovieFilesWidget::onViewUpdated()
{
    int movieCount = Manager::instance()->movieModel()->rowCount();
    int visibleCount = m_movieProxyModel->rowCount();
    if (movieCount == visibleCount) {
        ui->statusLabel->setText(tr("%n movies", "", movieCount));
    } else {
        ui->statusLabel->setText(tr("%1 of %n movies", "", movieCount).arg(visibleCount));
    }
}

void MovieFilesWidget::playMovie(QModelIndex idx)
{
    if (!idx.isValid()) {
        return;
    }
    QString fileName = m_movieProxyModel->data(idx, Qt::UserRole + 7).toString();
    if (fileName.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
}
