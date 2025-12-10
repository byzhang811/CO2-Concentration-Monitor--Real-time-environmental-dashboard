#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QTimer>
#include <QPainter>
#include <QDateTime>
#include <QVector>
#include <QPolygon>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QScreen>
#include <QLinearGradient>
#include <algorithm>
#include <numeric>
#include <cstdlib> 
#include <QFile>
#include <QTextStream> 
#include <QString>
extern "C" int init_ccs811(void);
extern "C" int read_co2_ppm(void);

// ----- GPIO configuration -----
static const int RED_GPIO   = 60;   // J2 pin 6
static const int GREEN_GPIO = 48;   // J2 pin 5

static void initLedGpio()
{
    // Export GPIOs (ignore error if already exported)
    system("echo 60 > /sys/class/gpio/export 2>/dev/null");
    system("echo 48 > /sys/class/gpio/export 2>/dev/null");

    // Set direction to output
    system("echo out > /sys/class/gpio/gpio60/direction 2>/dev/null");
    system("echo out > /sys/class/gpio/gpio48/direction 2>/dev/null");

    // Start with both LEDs off
    system("echo 0 > /sys/class/gpio/gpio60/value 2>/dev/null");
    system("echo 0 > /sys/class/gpio/gpio48/value 2>/dev/null");
}

static void setAirQualityLeds(bool redOn, bool greenOn)
{
    if (redOn)
        system("echo 1 > /sys/class/gpio/gpio60/value 2>/dev/null");
    else
        system("echo 0 > /sys/class/gpio/gpio60/value 2>/dev/null");

    if (greenOn)
        system("echo 1 > /sys/class/gpio/gpio48/value 2>/dev/null");
    else
        system("echo 0 > /sys/class/gpio/gpio48/value 2>/dev/null");
}

// -------- Screen Saver Widget (bouncing glowing text, with margins) --------
class ScreenSaverWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScreenSaverWidget(QWidget *parent = nullptr)
        : QWidget(parent),
          x(40),
          y(40),
          dx(3),
          dy(2),
          margin(10),
          titleText("CO2 MONITOR"),
          subtitleText("Touch to wake")
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAutoFillBackground(false);

        // Scale fonts based on screen height (same idea as main UI)
        int H = QApplication::primaryScreen()->size().height();  // 272 on your LCD
        double s = H > 0 ? (double)H / 480.0 : 1.0;
        int titleSize    = std::max(18, int(32 * s));
        int subtitleSize = std::max(10, int(14 * s));

        titleFont.setPointSize(titleSize);
        titleFont.setBold(true);
        subtitleFont.setPointSize(subtitleSize);

        updateTextMetrics();

        animTimer = new QTimer(this);
        connect(animTimer, &QTimer::timeout, this, &ScreenSaverWidget::step);
        animTimer->start(50); // ~20 FPS
    }

signals:
    void userActivity();  // emitted when user taps the screen saver

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Background gradient
        QLinearGradient grad(rect().topLeft(), rect().bottomRight());
        grad.setColorAt(0.0, QColor("#050814"));
        grad.setColorAt(1.0, QColor("#000000"));
        p.fillRect(rect(), grad);

        // Soft glow behind text
        QPoint center(x + textWidth / 2, y + textHeight / 2);
        QRadialGradient glow(center, textWidth * 0.8);
        glow.setColorAt(0.0, QColor(0, 230, 150, 120));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, int(textWidth * 0.7), int(textHeight));

        // Draw title
        p.setPen(QColor("#00e676"));
        p.setFont(titleFont);
        QFontMetrics fmTitle(titleFont);

        int titleX = x + (textWidth - titleWidth) / 2;
        int titleY = y + fmTitle.ascent();

        // Simple glow effect (outline)
        p.setPen(QColor(0, 230, 150, 80));
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                p.drawText(titleX + dx, titleY + dy, titleText);
            }
        }

        p.setPen(QColor("#00ffa0"));
        p.drawText(titleX, titleY, titleText);

        // Draw subtitle
        p.setFont(subtitleFont);
        QFontMetrics fmSub(subtitleFont);
        int subX = x + (textWidth - subtitleWidth) / 2;
        int subY = y + fmTitle.height() + spacing + fmSub.ascent();
        p.setPen(QColor(220, 230, 255, 230));
        p.drawText(subX, subY, subtitleText);
    }

    void mousePressEvent(QMouseEvent *event) override {
        Q_UNUSED(event);
        emit userActivity();
        // Do not propagate click to underlying widgets
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        // Ensure still inside bounds if window size changes
        clampPosition();
    }

private slots:
    void step() {
        x += dx;
        y += dy;

        QRect r = rect();
        int maxX = r.width()  - margin - textWidth;
        int maxY = r.height() - margin - textHeight;

        // Bounce horizontally
        if (x < margin) {
            x = margin;
            dx = -dx;
        } else if (x > maxX) {
            x = maxX;
            dx = -dx;
        }

        // Bounce vertically
        if (y < margin) {
            y = margin;
            dy = -dy;
        } else if (y > maxY) {
            y = maxY;
            dy = -dy;
        }

        update();
    }

private:
    void updateTextMetrics() {
        QFontMetrics fmTitle(titleFont);
        QFontMetrics fmSub(subtitleFont);

        titleWidth  = fmTitle.horizontalAdvance(titleText);
        subtitleWidth = fmSub.horizontalAdvance(subtitleText);

        spacing    = std::max(4, fmSub.height() / 3);
        textWidth  = std::max(titleWidth, subtitleWidth);
        textHeight = fmTitle.height() + spacing + fmSub.height();
    }

    void clampPosition() {
        QRect r = rect();
        int maxX = r.width()  - margin - textWidth;
        int maxY = r.height() - margin - textHeight;

        if (x < margin) x = margin;
        if (y < margin) y = margin;
        if (x > maxX)   x = maxX;
        if (y > maxY)   y = maxY;
    }

    // Position & movement
    int x, y;
    int dx, dy;
    int margin;

    // Text & fonts
    QString titleText;
    QString subtitleText;
    QFont titleFont;
    QFont subtitleFont;

    // Cached geometry
    int titleWidth;
    int subtitleWidth;
    int textWidth;
    int textHeight;
    int spacing;

    QTimer *animTimer;
};

// -------- Trend Plot Widget --------
class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(double s = 1.0, QWidget *parent = nullptr)
        : QWidget(parent), scale(s)
    {}

    void addSample(int value) {
        samples.append(value);
        if (samples.size() > maxPoints)
            samples.removeFirst();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);

        // Background gradient
        QLinearGradient grad(rect().topLeft(), rect().bottomRight());
        grad.setColorAt(0.0, QColor("#101525"));
        grad.setColorAt(1.0, QColor("#050812"));
        p.fillRect(rect(), grad);

        if (samples.isEmpty())
            return;

        int w = width();
        int h = height();

        // Layout margins for axes and header text
        const int leftMargin   = 40;   // space for Y-axis labels
        const int rightMargin  = 12;
        const int topMargin    = 32;   // space for title + min/avg/max
        const int bottomMargin = 18;

        int plotW = w - leftMargin - rightMargin;
        int plotH = h - topMargin - bottomMargin;
        if (plotW <= 0 || plotH <= 0)
            return;

        // Data range
        int maxVal = *std::max_element(samples.begin(), samples.end());
        int minVal = *std::min_element(samples.begin(), samples.end());
        if (maxVal == minVal) {
            maxVal += 10;
            minVal -= 10;
        }

        // 60s average (all current samples)
        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        double avg = sum / samples.size();

        // ----- Draw background grid in plot area -----
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QColor(255, 255, 255, 30));

        int gridLines = 3;
        for (int i = 0; i <= gridLines; ++i) {
            int gy = topMargin + i * plotH / gridLines;
            p.drawLine(leftMargin, gy, leftMargin + plotW, gy);
        }

        // ----- Draw Y-axis with ticks and labels -----
        p.setPen(QColor(255, 255, 255, 120));
        p.drawLine(leftMargin, topMargin, leftMargin, topMargin + plotH);  // main axis

        QFont axisFont = p.font();
        int axisFontSize = std::max(8, int(10 * scale));
        axisFont.setPointSize(axisFontSize);
        p.setFont(axisFont);

        int tickCount = 4;  // 0%, 33%, 66%, 100%
        for (int i = 0; i <= tickCount; ++i) {
            double t = double(i) / tickCount;
            int yTick = topMargin + (1.0 - t) * plotH;

            // small tick mark
            p.drawLine(leftMargin - 4, yTick, leftMargin, yTick);

            // label value
            int val = minVal + t * (maxVal - minVal);
            QString label = QString::number(val);
            QRect textRect(0, yTick - axisFontSize, leftMargin - 6, axisFontSize * 2);
            p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }

        // ----- Polyline for raw values -----
        QPolygon poly;
        int n = samples.size();
        for (int i = 0; i < n; i++) {
            double x = leftMargin + (double)i / (n - 1) * plotW;
            double norm = (samples[i] - minVal) / double(maxVal - minVal);
            double y = topMargin + (1.0 - norm) * plotH;
            poly << QPoint((int)x, (int)y);
        }

        // Raw data line
        p.setPen(QColor("#00e676"));
        p.drawPolyline(poly);

        // ----- Average horizontal line -----
        double normAvg = (avg - minVal) / double(maxVal - minVal);
        double yAvg = topMargin + (1.0 - normAvg) * plotH;
        p.setPen(QColor("#ffeb3b"));   // yellow line
        p.drawLine(leftMargin, (int)yAvg, leftMargin + plotW, (int)yAvg);

        // ----- Border around whole widget -----
        p.setPen(QColor(255, 255, 255, 60));
        p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 12, 12);

        // ----- Header text (outside the plot area, not盖在线上) -----
        QFont titleFont = p.font();
        int titleSize = std::max(8, int(12 * scale));
        titleFont.setPointSize(titleSize);
        titleFont.setBold(true);
        p.setFont(titleFont);

        QString header1 = "CO2 Trend (ppm)";
        p.setPen(Qt::white);
        p.drawText(leftMargin, topMargin - 14, header1);

        QFont statsFont = p.font();
        int statsSize = std::max(8, int(10 * scale));
        statsFont.setPointSize(statsSize);
        statsFont.setBold(false);
        p.setFont(statsFont);

        QString header2 = QString("Min: %1   Avg(60s): %2   Max: %3")
                              .arg(minVal)
                              .arg((int)avg)
                              .arg(maxVal);
        p.setPen(QColor(200, 220, 255));
        p.drawText(leftMargin, topMargin - 2, header2);
    }

private:
    QVector<int> samples;
    const int maxPoints = 60;   // last ~60 seconds
    double scale;
};


class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QWidget(parent),
          screenSaver(nullptr),
          idleTimer(nullptr),
          inScreenSaver(false),
          logFile(nullptr)
    {
        // ==== Auto scale based on screen height (reference 480) ====
        int H = QApplication::primaryScreen()->size().height();
        scale = H > 0 ? (double)H / 480.0 : 1.0;

        co2FontSize    = std::max(24, int(56 * scale));
        buttonFontSize = std::max(12, int(18 * scale));
        statusFontSize = std::max(10, int(14 * scale));
        titleFontSize  = std::max(14, int(22 * scale));
        hintFontSize   = std::max(8,  int(10 * scale));

        // Global background
        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor("#050814"));
        setPalette(pal);

        // Title bar
        titleLabel = new QLabel("CO2 Environment Monitor");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(
            QString("font-size:%1px; font-weight:bold; color:#ffffff; padding:4px 0;")
                .arg(titleFontSize));

        // Dashboard card
        QWidget *card = new QWidget;
        card->setObjectName("card");
        card->setStyleSheet(
            "#card { background-color: rgba(13, 19, 40, 230);"
            "border-radius: 14px; border: 1px solid rgba(255,255,255,40); }");

        co2Label = new QLabel("CO2: -- ppm");
        co2Label->setAlignment(Qt::AlignCenter);
        co2Label->setStyleSheet(
            QString("font-size:%1px; font-weight:600; color:#00e676;")
                .arg(co2FontSize));

        statusLabel = new QLabel("Initializing sensor...");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setWordWrap(true);
        statusLabel->setMaximumWidth(440);
        statusLabel->setStyleSheet(
            QString("font-size:%1px; color:#d0d4ff;").arg(statusFontSize));

        QLabel *hintLabel = new QLabel("Tap the screen to keep the display awake.");
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setStyleSheet(
            QString("font-size:%1px; color:rgba(255,255,255,120);")
                .arg(hintFontSize));

        QPushButton *showTrendBtn = new QPushButton("Show Trend");
        QPushButton *exitBtn      = new QPushButton("Exit");

        int radiusPx  = buttonFontSize;
        int padV      = std::max(4, buttonFontSize / 3);
        int padH      = std::max(8, buttonFontSize);

        QString btnStyle =
            QString(
                "QPushButton { background-color:#1b2340; color:#ffffff;"
                "border-radius:%1px; padding:%2px %3px; font-size:%4px; }"
                "QPushButton:hover  { background-color:#28345d; }"
                "QPushButton:pressed{ background-color:#12172b; }")
                .arg(radiusPx).arg(padV).arg(padH).arg(buttonFontSize);

        showTrendBtn->setStyleSheet(btnStyle);
        exitBtn->setStyleSheet(btnStyle);

        QHBoxLayout *buttonRow = new QHBoxLayout;
        buttonRow->setSpacing(12);
        buttonRow->addStretch();
        buttonRow->addWidget(showTrendBtn);
        buttonRow->addWidget(exitBtn);
        buttonRow->addStretch();

        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 16, 16, 16);
        cardLayout->setSpacing(10);
        cardLayout->addWidget(co2Label);
        cardLayout->addWidget(statusLabel);
        cardLayout->addWidget(hintLabel);
        cardLayout->addSpacing(4);
        cardLayout->addLayout(buttonRow);

        QWidget *dashboardPage = new QWidget;
        QVBoxLayout *dashLayout = new QVBoxLayout(dashboardPage);
        dashLayout->setContentsMargins(8, 8, 8, 8);
        dashLayout->addStretch();
        dashLayout->addWidget(card, 0, Qt::AlignHCenter);
        dashLayout->addStretch();

        // Trend Page
        plotWidget = new PlotWidget(scale);
        plotWidget->setMinimumHeight(160);

        QLabel *trendHint = new QLabel("Recent CO2 history (last ~60 seconds).");
        trendHint->setAlignment(Qt::AlignCenter);
        trendHint->setStyleSheet(
            QString("font-size:%1px; color:#b0b5ff;").arg(statusFontSize));

        QPushButton *backBtn = new QPushButton("Back to Dashboard");
        backBtn->setStyleSheet(btnStyle);

        QWidget *trendPage = new QWidget;
        QVBoxLayout *trendLayout = new QVBoxLayout(trendPage);
        trendLayout->setContentsMargins(12, 12, 12, 12);
        trendLayout->setSpacing(8);
        trendLayout->addWidget(plotWidget, 1);
        trendLayout->addWidget(trendHint);
        trendLayout->addWidget(backBtn, 0, Qt::AlignCenter);

        // Page Manager
        stack = new QStackedWidget;
        stack->addWidget(dashboardPage);
        stack->addWidget(trendPage);

        // Root layout
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(stack, 1);

        // Buttons
        connect(showTrendBtn, &QPushButton::clicked, this, &MainWindow::showTrendPage);
        connect(backBtn,      &QPushButton::clicked, this, &MainWindow::showDashboardPage);
        connect(exitBtn, &QPushButton::clicked, this, []() {
            // Turn off LEDs
            setAirQualityLeds(false, false);

            // Optionally unexport GPIOs so they are clean next startup
            system("echo 60 > /sys/class/gpio/unexport 2>/dev/null");
            system("echo 48 > /sys/class/gpio/unexport 2>/dev/null");

            // Clear framebuffer
            system("dd if=/dev/zero of=/dev/fb0 bs=1024 count=512 >/dev/null 2>&1");

            // Re-bind console so text TTY comes back
            system("echo 1 > /sys/class/vtconsole/vtcon1/bind");

            qApp->quit();
        });

        // ==== Sensor init ====
        if (init_ccs811() != 0) {
            statusLabel->setText("Sensor initialization failed.");
        } else {
            statusLabel->setText("Sensor OK. Waiting for data...");
        }

        initLedGpio();

        // ==== CSV LOGGING ====
        logFile = new QFile("/root/co2_log.csv", this);
        if (logFile->open(QIODevice::Append | QIODevice::Text)) {
            logStream.setDevice(logFile);
            if (logFile->size() == 0) {
                logStream << "timestamp,co2_ppm\n";
                logStream.flush();
            }
        } else {
            statusLabel->setText("Sensor OK, but failed to open log file.");
        }

        // ==== Timer ====
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateSensor);
        timer->start(1000);

        // ==== ScreenSaver ====
        screenSaver = new ScreenSaverWidget(this);
        screenSaver->setGeometry(rect());
        screenSaver->hide();
        screenSaver->raise();

        connect(screenSaver, &ScreenSaverWidget::userActivity,
                this, &MainWindow::onScreenSaverUserActivity);

        idleTimer = new QTimer(this);
        idleTimer->setInterval(15000);
        connect(idleTimer, &QTimer::timeout, this, &MainWindow::startScreenSaver);
        idleTimer->start();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (screenSaver) screenSaver->setGeometry(rect());
    }

    void mousePressEvent(QMouseEvent *event) override {
        idleTimer->start(15000);
        QWidget::mousePressEvent(event);
    }

private slots:
    void updateSensor() {
        int v = read_co2_ppm();
        if (v < 0) {
            statusLabel->setText("Read error.");
            return;
        }

        QString tsLog = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString t     = QDateTime::currentDateTime().toString("hh:mm:ss");

        // Update CO2 text
        co2Label->setText(QString("CO2: %1 ppm").arg(v));

        QString quality;
        if (v > 1500) {
            co2Label->setStyleSheet(QString("font-size:%1px; color:#ff5252;").arg(co2FontSize));
            quality = "Poor";
        } else if (v > 1000) {
            co2Label->setStyleSheet(QString("font-size:%1px; color:#ffb300;").arg(co2FontSize));
            quality = "Moderate";
        } else if (v > 800) {
            co2Label->setStyleSheet(QString("font-size:%1px; color:#ffeb3b;").arg(co2FontSize));
            quality = "Fair";
        } else {
            co2Label->setStyleSheet(QString("font-size:%1px; color:#00e676;").arg(co2FontSize));
            quality = "Good";
        }

        statusLabel->setText(
            QString("Air Quality: %1\nUpdated at %2")
                .arg(quality).arg(t));

        plotWidget->addSample(v);
        
        // ----- External LED logic -----
        if (v > 3000) {
            setAirQualityLeds(true, false);   // red ON, green OFF
        } else {
            setAirQualityLeds(false, true);   // green ON, red OFF
        }

        // ==== WRITE CSV LOG ====
        if (logFile && logFile->isOpen()) {
            logStream << tsLog << "," << v << "\n";
            logStream.flush();
        }
    }

    void showTrendPage() { stack->setCurrentIndex(1); }
    void showDashboardPage() { stack->setCurrentIndex(0); }

    void startScreenSaver() {
        inScreenSaver = true;
        screenSaver->show();
        screenSaver->raise();
    }

    void stopScreenSaver() {
        inScreenSaver = false;
        screenSaver->hide();
    }

    void onScreenSaverUserActivity() {
        stopScreenSaver();
        idleTimer->start(15000);
    }

private:
    QLabel *titleLabel;
    QLabel *co2Label;
    QLabel *statusLabel;
    QStackedWidget *stack;
    PlotWidget *plotWidget;
    QTimer *timer;

    ScreenSaverWidget *screenSaver;
    QTimer *idleTimer;
    bool inScreenSaver;

    // ===== CSV LOGGING =====
    QFile *logFile;
    QTextStream logStream;

    // Scaling
    double scale;
    int co2FontSize;
    int buttonFontSize;
    int statusFontSize;
    int titleFontSize;
    int hintFontSize;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.showFullScreen();
    return app.exec();
}

#include "main.moc"
