#include "ui/notifications/NotificationBox.h"
#include "ui_NotificationBox.h"

#include <QDebug>
#include <QLabel>

NotificationBox::NotificationBox(QWidget* parent) : QWidget(parent), ui(new Ui::NotificationBox)
{
    ui->setupUi(this);
    m_msgCounter = 0;
    hide();
}

/**
 * @brief NotificationBox::~NotificationBox
 */
NotificationBox::~NotificationBox()
{
    delete ui;
}

/**
 * @brief Returns an instance of the message box
 * @param parent Parent widget (used when called the first time)
 * @return Instance of message box
 */
NotificationBox* NotificationBox::instance(QWidget* parent)
{
    static NotificationBox* m_instance = nullptr;
    if (m_instance == nullptr) {
        m_instance = new NotificationBox(parent);
    }
    return m_instance;
}

/**
 * @brief Repositions the message box in the upper right corner
 * @param size Size of the parent widget (MainWidget)
 */
void NotificationBox::reposition(QSize size)
{
    move(size.width() - this->size().width(), 40);
    m_parentSize = size;
}

/**
 * @brief Adjusts the size to hold all the messages
 */
void NotificationBox::adjustSize()
{
    int height = 48;
    for (const Message* msg : m_messages) {
        height += msg->sizeHint().height() + ui->layoutMessages->spacing();
    }
    height -= ui->layoutMessages->spacing();
    resize(this->size().width(), height);
}

/**
 * @todo: use type to display different styles
 */
int NotificationBox::showMessage(QString message, NotificationBox::NotificationType type, int timeout)
{
    m_msgCounter++;
    auto msg = new Message(this);
    msg->setMessage(message, timeout);
    msg->setType(type);
    msg->setId(m_msgCounter);
    m_messages.append(msg);
    ui->layoutMessages->addWidget(msg);
    adjustSize();
    show();
    connect(msg, &Message::sigHideMessage, this, &NotificationBox::removeMessage);
    // QApplication::processEvents(QEventLoop::WaitForMoreEvents);
    return m_msgCounter;
}

/**
 * @brief Removes a message
 * @param id Id of the message to remove
 */
void NotificationBox::removeMessage(int id)
{
    qDebug() << "Entered, id=" << id;
    for (Message* msg : m_messages) {
        if (msg->id() == id) {
            ui->layoutMessages->removeWidget(msg);
            msg->deleteLater();
            m_messages.removeOne(msg);
            adjustSize();
        }
    }
    if (m_messages.empty()) {
        hide();
    }
}

/**
 * @brief Shows a message with progress bar
 * @param message Message to display
 * @param id Id of the message
 */
void NotificationBox::showProgressBar(QString message, int id, bool unique)
{
    qDebug() << "Entered, message=" << message << "id=" << id;
    if (unique) {
        for (Message* msg : m_messages) {
            if (msg->id() == id) {
                return;
            }
        }
    }
    m_msgCounter++;
    auto* msg = new Message(this);
    msg->setMessage(message);
    msg->setId(id);
    msg->showProgressBar(true);
    m_messages.append(msg);
    adjustSize();
    ui->layoutMessages->addWidget(msg);
    show();
    connect(msg, &Message::sigHideMessage, this, &NotificationBox::removeMessage);
}

int NotificationBox::addProgressBar(QString message)
{
    m_msgCounter++;
    int id = m_msgCounter;
    showProgressBar(message, id, true);
    return id;
}

/**
 * @brief Sets the value of a message with progress bar
 * @param current Current value
 * @param max Maximum value
 * @param id Id of the message
 */
void NotificationBox::progressBarProgress(int current, int max, int id)
{
    for (Message* msg : m_messages) {
        if (msg->id() == id) {
            msg->setProgress(current, max);
        }
    }
}

/**
 * @brief Hides a message with progress bar
 * @param id Id of message to hide
 */
void NotificationBox::hideProgressBar(int id)
{
    qDebug() << "[NotificationBox] Hide Progress bar with id" << id;
    removeMessage(id);
}

int NotificationBox::maxValue(int id)
{
    for (Message* msg : m_messages) {
        if (msg->id() == id) {
            return msg->maxValue();
        }
    }

    return 0;
}

int NotificationBox::value(int id)
{
    for (Message* msg : m_messages) {
        if (msg->id() == id) {
            return msg->value();
        }
    }

    return 0;
}
