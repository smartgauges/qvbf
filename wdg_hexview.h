#ifndef WDG_HEXVIEW_H
#define WDG_HEXVIEW_H

#include <QAbstractScrollArea>
#include <QByteArray>

class wdg_hexview: public QAbstractScrollArea
{
	public:
		wdg_hexview(QWidget *parent = 0);
		~wdg_hexview();

	public slots:
		void setData(const QByteArray * data);

	protected:
		void paintEvent(QPaintEvent *event);

	private:
		const QByteArray * m_data;
		std::size_t m_posAddr; 
		std::size_t m_posHex;
		std::size_t m_posAscii;
		std::size_t m_charWidth;
		std::size_t m_charHeight;

		std::size_t m_cursorPos;

		QSize fullSize() const;
};

#endif
