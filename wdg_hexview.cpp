// based on https://github.com/virinext/QHexView

#include <QScrollBar>
#include <QPainter>
#include <QSize>
#include <QPaintEvent>
#include <QApplication>

#include <QDebug>

#include "wdg_hexview.h"

const int HEXCHARS_IN_LINE = 47;
const int GAP_ADR_HEX = 10;
const int GAP_HEX_ASCII = 16;
const int BYTES_PER_LINE = 16;

wdg_hexview::wdg_hexview(QWidget *parent) : QAbstractScrollArea(parent)
{
	m_data = NULL;
	setFont(QFont("Courier", 10));

	//m_charWidth = fontMetrics().horizontalAdvance(QLatin1Char('9'));
	m_charWidth = fontMetrics().width(QLatin1Char('9'));
	m_charHeight = fontMetrics().height();

	m_posAddr = 0;
	m_posHex = 10 * m_charWidth + GAP_ADR_HEX;
	m_posAscii = m_posHex + HEXCHARS_IN_LINE * m_charWidth + GAP_HEX_ASCII;

	setMinimumWidth(m_posAscii + (BYTES_PER_LINE * m_charWidth));

	setFocusPolicy(Qt::StrongFocus);
}

wdg_hexview::~wdg_hexview()
{
}

void wdg_hexview::setData(const QByteArray * data)
{
	verticalScrollBar()->setValue(0);
	m_data = data;
	viewport()->update();
}

QSize wdg_hexview::fullSize() const
{
	std::size_t size = m_data ? m_data->size() : 0;
	std::size_t width = m_posAscii + (BYTES_PER_LINE * m_charWidth);
	std::size_t height = size / BYTES_PER_LINE;
	if (size % BYTES_PER_LINE)
		height++;

	height *= m_charHeight;

	return QSize(width, height);
}

void wdg_hexview::paintEvent(QPaintEvent *event)
{
	QPainter painter(viewport());
	std::size_t size = m_data ? m_data->size() : 0;
	QSize areaSize = viewport()->size();
	QSize widgetSize = fullSize();
	verticalScrollBar()->setPageStep(areaSize.height() / m_charHeight);
	verticalScrollBar()->setRange(0, (widgetSize.height() - areaSize.height()) / m_charHeight + 1);

	int firstLineIdx = verticalScrollBar() -> value();

	int lastLineIdx = firstLineIdx + areaSize.height() / m_charHeight;
	if (lastLineIdx > size / BYTES_PER_LINE)
	{
		lastLineIdx = size / BYTES_PER_LINE;
		if (size % BYTES_PER_LINE)
			lastLineIdx++;
	}

	painter.fillRect(event->rect(), this->palette().color(QPalette::Base));

	QColor addressAreaColor = QColor(0xd4, 0xd4, 0xd4, 0xff);
	painter.fillRect(QRect(m_posAddr, event->rect().top(), m_posHex - GAP_ADR_HEX + 2 , height()), addressAreaColor);

	int linePos = m_posAscii - (GAP_HEX_ASCII / 2);
	painter.setPen(Qt::gray);

	painter.drawLine(linePos, event->rect().top(), linePos, height());

	painter.setPen(Qt::black);

	int yPosStart = m_charHeight;

	QBrush def = painter.brush();

	QByteArray data;
	if (m_data)
		data = m_data->mid(firstLineIdx * BYTES_PER_LINE, (lastLineIdx - firstLineIdx) * BYTES_PER_LINE);

	for (int lineIdx = firstLineIdx, yPos = yPosStart;  lineIdx < lastLineIdx; lineIdx += 1, yPos += m_charHeight)
	{
		QString address = QString("%1").arg(lineIdx * 16, 10, 16, QChar('0'));
		painter.drawText(m_posAddr, yPos, address);

		for(int xPos = m_posHex, i=0; i<BYTES_PER_LINE && ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < data.size(); i++, xPos += 3 * m_charWidth)
		{
			QString val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF0) >> 4, 16);
			painter.drawText(xPos, yPos, val);

			painter.setBackground(def);
			painter.setBackgroundMode(Qt::OpaqueMode);

			val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF), 16);
			painter.drawText(xPos+m_charWidth, yPos, val);

			painter.setBackground(def);
			painter.setBackgroundMode(Qt::OpaqueMode);

		}

		for (int xPosAscii = m_posAscii, i=0; ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < data.size() && (i < BYTES_PER_LINE); i++, xPosAscii += m_charWidth)
		{
			char ch = data[(lineIdx - firstLineIdx) * BYTES_PER_LINE + i];
			if ((ch < 0x20) || (ch > 0x7e))
				ch = '.';

			painter.drawText(xPosAscii, yPos, QString(ch));
		}

	}
}

