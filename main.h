#ifndef MAIN_H
#define MAIN_H

#include <QMainWindow>
#include <QItemSelection>

#include "vbfmodel.h"

namespace Ui {
	class main;
}

class main_t : public QMainWindow
{
	Q_OBJECT

	public:
		explicit main_t(QWidget *parent = 0);
		~main_t();
		void open_file_vbf(const QString & fileName);

	private:
		int get_selected_row();
		void load_header();

	private slots:
		void slt_btn_open();
		void slt_btn_save();
		void slt_btn_export();
		void slt_btn_import();
		void slt_btn_add();
		void slt_view_clicked(const QModelIndex & idx);
		void slt_selection_changed(const QItemSelection & selection);
		void slt_btn_block_open();
		void slt_btn_block_save();
		void slt_header_changed();
		void slt_block_changed();
		void slt_btn_about(int);

	private:
		Ui::main *m_ui;

		VbfModel list;
};

#endif

