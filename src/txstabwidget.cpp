#include "txstabwidget.h"
#include <QColor>
#include <QEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionTab>
#include <QWheelEvent>
#include "latexeditorview.h"
#include "latexdocument.h"
#include "smallUsefulFunctions.h"

TxsTabWidget::TxsTabWidget(QWidget *parent) :
	QTabWidget(parent),
	m_active(false)
{
	setFocusPolicy(Qt::ClickFocus);
	setContextMenuPolicy(Qt::PreventContextMenu);

	ChangeAwareTabBar *tb = new ChangeAwareTabBar();
	tb->setContextMenuPolicy(Qt::CustomContextMenu);
	tb->setUsesScrollButtons(true);
    //tb->setAutoHide(true);
	connect(tb, SIGNAL(customContextMenuRequested(QPoint)), this, SIGNAL(tabBarContextMenuRequested(QPoint)));
    connect(tb, SIGNAL(currentTabAboutToChange(int,int)), this, SLOT(currentTabAboutToChange(int,int)));
	connect(tb, SIGNAL(tabLeftClicked()), this, SIGNAL(activationRequested()));
    //connect(tb, SIGNAL(middleMouseButtonPressed(int)), this, SLOT(tabCloseRequest(int)));
    connect(this, SIGNAL(tabCloseRequested(int)), this, SLOT(onTabCloseRequest(int)));
	setTabBar(tb);
	// Observe hover over the whole tab bar, not only over the selected tab.
	tb->installEventFilter(this);

    setDocumentMode(true);
    const QTabBar *tb2 = tabBar();
    connect(tb2, SIGNAL(tabMoved(int,int)), this, SIGNAL(tabMoved(int,int)));
    setProperty("tabsClosable", true);
    setProperty("movable", true);

	connect(this, SIGNAL(currentChanged(int)), this, SIGNAL(currentEditorChanged()));
}

// Change the tab captions only: do not change QTabWidget or editor widget fonts.
void TxsTabWidget::setEditorTabFont(const QFont &font)
{
    tabBar()->setFont(font);
}

// Only change the background of the currently selected editor tab.
// An empty or invalid value preserves the native application theme.
void TxsTabWidget::setActiveTabColor(const QString &color)
{
    if (m_activeTabColor == color)
        return;
    m_activeTabColor = color;
    updateStyle();
}

// Unchecked preserves the original always-colored selected tab behavior.
void TxsTabWidget::setActiveTabColorOnHover(bool onlyWhenMouseOver)
{
    if (m_activeTabColorOnHover == onlyWhenMouseOver)
        return;
    m_activeTabColorOnHover = onlyWhenMouseOver;
    updateStyle();
}

bool TxsTabWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar() &&
            (event->type() == QEvent::Enter || event->type() == QEvent::Leave)) {
        const bool hovered = event->type() == QEvent::Enter;
        if (m_tabBarHovered != hovered) {
            m_tabBarHovered = hovered;
            auto *bar = static_cast<ChangeAwareTabBar *>(tabBar());
            bar->setBarHovered(hovered); // Repaint only; leave the scroll offset intact.
            if (m_activeTabColorOnHover && hovered)
                updateStyle(); // On entering, reveal the selected tab as before.
            // Do NOT call updateStyle() on Leave: resetting the Qt stylesheet
            // causes QTabBar to scroll the selected tab back into view.
        }
    }
    return QTabWidget::eventFilter(watched, event);
}

void TxsTabWidget::moveTab(int from, int to)
{
	int cur = currentIndex();
	QString text = tabText(from);
	QWidget *wdg = widget(from);
	removeTab(from);
	insertTab(to, wdg, text);
	if (cur == from) setCurrentIndex(to);
	else if (from < to && cur >= from && cur < to)
		setCurrentIndex(cur - 1);
	else if (to < from && to && cur >= to && cur < from)
		setCurrentIndex(cur + 1);
}

QList<LatexEditorView *> TxsTabWidget::editors() const
{
	QList<LatexEditorView *> list;
	for (int i = 0; i < count(); i++) {
		LatexEditorView *edView = qobject_cast<LatexEditorView *>(widget(i));
		Q_ASSERT(edView); // there should only be editors as tabs
		list.append(edView);
	}
	return list;
}

bool TxsTabWidget::containsEditor(LatexEditorView *edView) const
{
	if (!edView) return false;
	return (indexOf(edView) >= 0);
}

LatexEditorView *TxsTabWidget::currentEditor() const
{
	return qobject_cast<LatexEditorView *>(currentWidget());
}

void TxsTabWidget::setCurrentEditor(LatexEditorView *edView)
{
	if (currentWidget() == edView)
		return;

	if (indexOf(edView) < 0) {
		// catch calls in which editor is not a member tab.
		// TODO: such calls are deprecated as bad practice. We should avoid them in the long run. For the moment the fallback to do nothing is ok.
		qDebug() << "Warning (deprecated call): TxsTabWidget::setCurrentEditor: editor not member of TxsTabWidget" << edView;

		return;
	}
	setCurrentWidget(edView);
}

LatexEditorView *TxsTabWidget::editorAt(QPoint p) {
	int index = tabBar()->tabAt(p);
    if (index < 0) return nullptr;
	return qobject_cast<LatexEditorView *>(widget(index));
}

/*! \brief Mark the widget as active.
 *
 * If there are multiple widgets, we want to visually indicate the active one,
 * i.e. the one containing the current editor.
 * We currently use bold on the current tab, but due to a Qt bug we're required
 * to increase the tab width all tabs in the active widget.
 * see https://bugreports.qt.io/browse/QTBUG-6905
 */
void TxsTabWidget::setActive(bool active) {
	if (active == m_active) return;
	m_active = active;
    updateStyle();
    if(active){
        if(currentEditor())
            currentEditor()->setFocus();
    }
}
/*!
 * \brief update style to change appearance of close buttons
 */
void TxsTabWidget::updateStyle()
{
    QString baseStyle="";
#ifdef Q_OS_MACOS
    if(interfaceStyle == "macOS" || interfaceStyle.isEmpty()){
        // work-around on mac for disapearing close button on tabs
        if(darkMode){
            baseStyle = "QTabBar::close-button {image: url(:/images-ng/close-tab_dm.svg)} QTabBar::close-button:hover {image: url(:/images-ng/close-tab-hover.svgz)}";
        }else{
            baseStyle = "QTabBar::close-button {image: url(:/images-ng/close-tab.svgz)} QTabBar::close-button:hover {image: url(:/images-ng/close-tab-hover.svgz)}";
        }
    }
#endif
    const QColor selectedColor(m_activeTabColor.trimmed());
    auto *bar = static_cast<ChangeAwareTabBar *>(tabBar());
    // In hover-only mode, paint the selected tab without resetting its stylesheet
    // on mouse leave. QTabBar resets its scroll position during stylesheet changes.
    bar->setHoverHighlightColor(m_active && m_activeTabColorOnHover
                                ? selectedColor : QColor());
    if (m_active) {
        // Apply the user color to the selected TAB only, not QTabWidget::pane
        // or the document editor. No extra rule when using the theme default.
        if (selectedColor.isValid() && !m_activeTabColorOnHover) {
            // Ensure the caption stays readable on light and dark custom colors.
            const QString textColor = qGray(selectedColor.rgb()) < 140
                    ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
            baseStyle += QStringLiteral(" QTabBar::tab:selected { background-color: %1; color: %2; }")
                    .arg(selectedColor.name(QColor::HexRgb), textColor);
        }
        setStyleSheet(baseStyle);
    } else {
        // In split-editor mode, do not highlight the inactive group's tab.
        setStyleSheet(baseStyle + " QTabBar {color: darkgrey;}");
    }
}

bool TxsTabWidget::isEmpty() const {
	return (count() == 0);
}

/*!
 * \brief check if active tab is the left most
 * \return
 */
bool TxsTabWidget::currentEditorViewIsFirst() const {
	return (currentIndex() == 0);
}
/*!
 * \brief check if active tab is the right most
 * \return
 */
bool TxsTabWidget::currentEditorViewIsLast() const {
	return (currentIndex() >= count()-1);
}

void TxsTabWidget::setInterfaceStyle(const QString style)
{
    interfaceStyle=style;
}
/*!
 * \brief activate tab to the right
 *
 * right most tab remains active, no roll over.
 */
void TxsTabWidget::gotoNextDocument()
{
	if (count() <= 1) return;
	int cPage = currentIndex() + 1;
	if (cPage >= count()) setCurrentIndex(0);
	else setCurrentIndex(cPage);
}
/*!
 * \brief activate tab to the left
 *
 * Left most tab (0) remains active, no roll over.
 */
void TxsTabWidget::gotoPrevDocument()
{
	if (count() <= 1) return;
	int cPage = currentIndex() - 1;
	if (cPage < 0) setCurrentIndex(count() - 1);
	else setCurrentIndex(cPage);
}
/*!
 * \brief activate first document/tab
 */
void TxsTabWidget::gotoFirstDocument() {
	if (count() <= 1) return;
	setCurrentIndex(0);
}
/*!
 * \brief activate last document/tab
 */
void TxsTabWidget::gotoLastDocument() {
	if (count() <= 1) return;
	setCurrentIndex(count()-1);
}

void TxsTabWidget::currentTabAboutToChange(int from, int to)
{
	LatexEditorView *edFrom = qobject_cast<LatexEditorView *>(widget(from));
	LatexEditorView *edTo = qobject_cast<LatexEditorView *>(widget(to));
	REQUIRE(edFrom);
	REQUIRE(edTo);
	emit editorAboutToChangeByTabClick(edFrom, edTo);
}

/*! \brief Handler for close requests coming from the tab bar.
 *
 * The widget cannot decide if the tab can really be closed, which can only be
 * determined at top level with user interaction if there are unsaved changes to
 * the editor. So we propagate the request up.
 * In the long terms one might consider asking the editor instead.
 */
void TxsTabWidget::onTabCloseRequest(int i)
{
	emit closeEditorRequested(editorAt(i));
}
/*!
 * \brief insert tab with given editor
 * \param edView editor
 * \param pos position, 0 is left most
 * \param asCurrent put editor as active into foreground
 */
void TxsTabWidget::insertEditor(LatexEditorView *edView, int pos, bool asCurrent)
{
	Q_ASSERT(edView);
	pos = insertTab(pos, edView, "?bug?");
	updateTab(pos);
	connectEditor(edView);

	if (asCurrent) setCurrentEditor(edView);
}
/*!
 * \brief remove tab which is connected to the given editor
 * \param edView
 */
void TxsTabWidget::removeEditor(LatexEditorView *edView)
{
	int i = indexOf(edView);
	if (i >= 0)
		removeTab(i);
	disconnectEditor(edView);
}

/*!
 * Returns the LatexEditorView at the given tab index or 0.
 */
LatexEditorView *TxsTabWidget::editorAt(int index)
{
	if (index < 0 || index >= count())
        return nullptr;
	return qobject_cast<LatexEditorView *>(widget(index));
}

/*!
 * Connect to signals of the editor so that the TabWidget will update the modified
 * status and the tab text when these properties of the editor change.
 *
 */
void TxsTabWidget::connectEditor(LatexEditorView *edView)
{
	connect(edView->editor, SIGNAL(contentModified(bool)), this, SLOT(updateTabFromSender()));
	connect(edView->editor, SIGNAL(readOnlyChanged(bool)), this, SLOT(updateTabFromSender()));
	connect(edView->editor, SIGNAL(titleChanged(QString)), this, SLOT(updateTabFromSender()));
}

/*!
 * Disconnect all connections from the editor to this.
 */
void TxsTabWidget::disconnectEditor(LatexEditorView *edView)
{
	edView->editor->disconnect(this);
}

/*!
 * Updates modified icon, tab text and tooltip.
 * TODO: tooltip should be dynamically generated when required because
 * this is not called always when root information changes.
 */
void TxsTabWidget::updateTab(int index)
{
	//cache icons, getRealIcon is *really* slow
	static QIcon readOnly = getRealIcon("document-locked");
	static QIcon modified = getRealIcon("modified");
	static QIcon empty = QIcon(":/images/empty.png");

	LatexEditorView *edView = editorAt(index);
	if (!edView) return;

	// update icon
	if (edView->editor->isReadOnly()) {
		setTabIcon(index, readOnly);
	} else if ((edView->editor->isContentModified())) {
		setTabIcon(index, modified);
	} else {
		setTabIcon(index, empty);
	}
	// update tab text
	setTabText(index, edView->displayNameForUI());
	// update tooltip text
	LatexDocument *doc = edView->document;
	LatexDocument *rootDoc = doc->getRootDocument();
	QString tooltip = QDir::toNativeSeparators(edView->editor->fileName());
	if (doc != rootDoc) {
		tooltip += tr("\nincluded document in %1").arg(rootDoc->getName());
	}
	setTabToolTip(index, tooltip);
}

/*!
 * Helper function. This is bound to editor signals and calls updateTab() on
 * the tab to which the sending editor belongs.
 */
void TxsTabWidget::updateTabFromSender()
{
	QEditor *editor = qobject_cast<QEditor *>(sender());
	for (int i = 0; i < count(); i++) {
		if (editorAt(i)->editor == editor) {
			updateTab(i);
			return;
		}
	}
}

// Repaint the selected tab's background without changing the QTabBar layout.
// The parent's stylesheet is deliberately left alone on mouse leave so a tab
// scrolled offscreen remains offscreen.
void ChangeAwareTabBar::setHoverHighlightColor(const QColor &color)
{
    if (m_hoverHighlightColor == color)
        return;
    m_hoverHighlightColor = color;
    update();
}

void ChangeAwareTabBar::setBarHovered(bool hovered)
{
    if (m_barHovered == hovered)
        return;
    m_barHovered = hovered;
    update();
}

void ChangeAwareTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    if (!m_barHovered || !m_hoverHighlightColor.isValid() || currentIndex() < 0)
        return;

    QStyleOptionTab option;
    initStyleOption(&option, currentIndex());
    const QRect selectedRect = option.rect.intersected(rect());
    if (selectedRect.isEmpty())
        return; // The selected tab is scrolled out of the visible bar.

    QPainter painter(this);
    painter.setClipRect(selectedRect);
    // Keep the native tab border. The normal close button is a child widget
    // and is not replaced by this paint operation.
    painter.fillRect(selectedRect.adjusted(1, 1, -1, -1), m_hoverHighlightColor);

    const QColor captionColor = qGray(m_hoverHighlightColor.rgb()) < 140
                                ? QColor(Qt::white) : QColor(Qt::black);
    option.palette.setColor(QPalette::WindowText, captionColor);
    option.palette.setColor(QPalette::ButtonText, captionColor);
    option.palette.setColor(QPalette::Text, captionColor);
    painter.setPen(captionColor);
    style()->drawControl(QStyle::CE_TabBarTabLabel, &option, &painter, this);
}

void ChangeAwareTabBar::mousePressEvent(QMouseEvent *event)
{
	int current = currentIndex();
	int toIndex = tabAt(event->pos());
	if (event->button() == Qt::LeftButton) {
		if (toIndex >= 0) {
			emit currentTabAboutToChange(current, toIndex);
		}
	}
	else if (event->button() == Qt::MiddleButton) {
		int tabNr = tabAt(event->pos());
		if (tabNr >= 0) {
			emit middleMouseButtonPressed(tabNr);
		}
        event->accept();
	}
	QTabBar::mousePressEvent(event);
	if (event->button() == Qt::LeftButton) {
		emit tabLeftClicked();
	}
}

// Move the tab strip with the mouse wheel, without changing the active editor.
// QTabBar's normal wheelEvent() switches the current tab; the built-in scroll
// arrow buttons instead move the visible tab range without selecting a file.
void ChangeAwareTabBar::wheelEvent(QWheelEvent *event)
{
    const QPoint angle = event->angleDelta();
    const int angularDelta = qAbs(angle.x()) > qAbs(angle.y()) ? angle.x() : angle.y();
    int steps = 0;

    if (angularDelta != 0) {
        // Preserve partial wheel notches, including high-resolution mouse wheels.
        m_wheelPixelRemainder = 0;
        m_wheelAngleRemainder += angularDelta;
        steps = m_wheelAngleRemainder / 120;
        m_wheelAngleRemainder %= 120;
    } else {
        // Touchpads can supply pixel deltas without an angular delta.
        const QPoint pixels = event->pixelDelta();
        const int pixelDelta = qAbs(pixels.x()) > qAbs(pixels.y()) ? pixels.x() : pixels.y();
        if (pixelDelta != 0) {
            m_wheelAngleRemainder = 0;
            m_wheelPixelRemainder += pixelDelta;
            steps = m_wheelPixelRemainder / 40;
            m_wheelPixelRemainder %= 40;
        }
    }

    if (steps != 0) {
        QToolButton *left = findChild<QToolButton *>(QStringLiteral("ScrollLeftButton"));
        QToolButton *right = findChild<QToolButton *>(QStringLiteral("ScrollRightButton"));

        // Qt 5 creates the same two buttons, but leaves their object names empty.
        // They are the first two direct QToolButton children, in left/right order.
        if (!left || !right) {
            const QList<QToolButton *> buttons =
                findChildren<QToolButton *>(QString(), Qt::FindDirectChildrenOnly);
            if (buttons.size() >= 2) {
                left = buttons.at(0);
                right = buttons.at(1);
            }
        }

        QToolButton *button = steps > 0 ? left : right;
        for (int i = 0; button && i < qAbs(steps); ++i) {
            if (!button->isVisible() || !button->isEnabled())
                break; // No overflow, or already at the end of the tab strip.
            button->click(); // Scroll tabs; never call setCurrentIndex().
        }
    }

    event->accept(); // Do not allow Qt to switch documents via this wheel event.
}
