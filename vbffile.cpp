#include <QFile>
#include <QtEndian>
#include <QBuffer>
#include <QDebug>

#include "vbffile.h"

//CRC-32 normal 0x04c11db7 or reverse 0xedb88320
static uint32_t crc32(const QByteArray & data)
{
	uint32_t poly = 0xedb88320;
	uint32_t table[256];
	for (int i = 0; i < 256; ++i) {

		uint32_t cr = i;
                for (int j = 8; j > 0; --j)
                        cr = cr & 0x00000001 ? (cr >> 1) ^ poly : (cr >> 1);
                table[i] = cr;
        }

	uint32_t crc = ~0U;
	for (int32_t i = 0; i < data.size(); i++)
		crc = (crc >> 8) ^ table[( crc ^ (data[i]) ) & 0xff];

	return ~crc;
}

//CRC-16/CCITT-FALSE
static uint16_t crc16(const QByteArray & data)
{
	uint16_t poly = 0x1021;
	uint16_t crc = 0xffff;
	for (int32_t i = 0; i < data.size(); i++) {

		crc ^= data[i] << 8;
		for (uint8_t j = 0; j < 8; j++) {

			if (crc & 0x8000)
				crc = (crc << 1) ^ poly;
			else
				crc = crc << 1;
		}
		crc &= 0xffff;
	}

    return crc;
}

bool vbf_open(const QString & fileName, vbf_t & vbf)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QByteArray data = file.readAll();

	//qDebug() << "file size:" << data.size();
	file.close();

	int offset = data.indexOf(";\r\n\r\n}");
	if (offset == -1) {

		offset = data.indexOf(";\r\n}");
		if (offset != -1)
			offset += 4;
	}
	else
		offset += 6;

	if (offset == -1) {

		//qDebug() << "cant find header";
		return false;
	}

	vbf.filename = fileName;

	vbf.header.data = data.left(offset);
	//qDebug() << "header size:" << vbf.header.data;

	QBuffer qbuf(&vbf.header.data);
	qbuf.open(QBuffer::ReadOnly);

	QString line;
	while (!qbuf.atEnd()) {

		QString l = qbuf.readLine();
		//qDebug() << l;

		if ((-1 != l.indexOf("//")) || (-1 != l.indexOf("header"))) {

			line.clear();
			continue;
		}

		line += l;

		//multiline
		if (-1 == line.indexOf(";"))
			continue;

		line = line.left(line.indexOf(";"));
		QStringList list = line.split(QRegExp("[\r\n\t\",{} ]+"), QString::SkipEmptyParts);

		qDebug() << line;
		line.clear();
		qDebug() << list;

		//qDebug() << line << list.size();
		if (list.size() < 3)
			continue;

		if (list[1] != QString('='))
			continue;

		if (list[0] == "sw_part_number")
			vbf.header.sw_part_number = list[2];

		if (list[0] == "sw_part_type")
			vbf.header.sw_part_type = list[2];

		if (list[0] == "can_frame_format")
			vbf.header.can_frame_format = list[2];

		if (list[0] == "network")
			vbf.header.network = list[2];

		bool ok;
		if (list[0] == "erase") {

			list.removeAt(0);
			list.removeAt(0);
			//qDebug() << list;

			for (int i = 0; i < list.size()/2; i++) {

				erase_t erase;
				erase.addr = list[2 * i].toLong(&ok, 16);
				erase.size = list[2 * i + 1].toLong(&ok, 16);
				vbf.header.erases.push_back(erase);
			}
		}

		if (list[0] == "ecu_address")
			vbf.header.ecu_address = list[2].toLong(&ok, 16);

		if (list[0] == "call")
			vbf.header.call = list[2].toLong(&ok, 16);

		if (list[0] == "file_checksum")
			vbf.header.file_checksum = list[2].toLong(&ok, 16);
	}
	qbuf.close();

	//qDebug() << vbf.header;

	data.remove(0, offset);

	qDebug() << "data crc32:" << hex << crc32(data) << vbf.header.file_checksum;

	vbf.size = 0;

	while (data.size()) {

		block_t block;

		block.addr = qFromBigEndian<quint32>(data.left(4).data());
		data.remove(0, 4);

		block.len = qFromBigEndian<quint32>(data.left(4).data());
		data.remove(0, 4);

		block.data = data.left(block.len);
		data.remove(0, block.len);

		uint16_t crc1 = qFromBigEndian<quint16>(data.left(2).data());
		data.remove(0, 2);

		uint16_t crc2 = crc16(block.data);

		qDebug() << "block addr:" << hex << block.addr << " len:" << block.len << " crc1:" << crc1 << " crc2:" << crc2;

		vbf.blocks.push_back(block);
		vbf.size += block.data.size();
	}

	return true;
}

bool vbf_add(const QString & fileName, vbf_t & vbf)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	block_t block;
	block.addr = 0;
	block.data = file.readAll();
	block.len = block.data.size();

	file.close();

	vbf.blocks.push_back(block);

	return true;
}

void vbf_export(const vbf_t & vbf)
{
	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		QString filename = vbf.filename + "." + QString::number(i) + ".bin";

		QFile file(filename);
		if (!file.open(QIODevice::WriteOnly))
			continue;

		file.write(vbf.blocks[i].data);
		file.close();
	}
}

void vbf_import(vbf_t & vbf)
{
	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		QString filename = vbf.filename + "." + QString::number(i) + ".bin";

		QFile file(filename);
		if (!file.open(QIODevice::ReadOnly))
			continue;

		QByteArray data = file.readAll();

		//qDebug() << "file size:" << data.size();
		file.close();

		block_t & block = vbf.blocks[i];
		block.data = data;
		block.len = data.size();
	}
}

void vbf_save(const QString & fileName, const vbf_t & vbf)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
		return;

	file.write(vbf.header.data);

	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		const block_t & block = vbf.blocks[i];

		uint32_t addr = qToBigEndian<quint32>(block.addr);
		file.write((const char *)&addr, sizeof(addr));

		uint32_t len = qToBigEndian<quint32>(block.len);
		file.write((const char *)&len, sizeof(len));

		file.write(vbf.blocks[i].data);

		uint16_t crc = crc16(block.data);
		crc = qToBigEndian<quint16>(crc);
		file.write((const char *)&crc, sizeof(crc));
	}

	file.close();
}

void vbf_update_header(vbf_t & vbf)
{
	QByteArray data;
	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		const block_t & block = vbf.blocks[i];

		uint32_t addr = qToBigEndian<quint32>(block.addr);
		QByteArray a((const char *)&addr, sizeof(addr));
		data += a;

		uint32_t len = qToBigEndian<quint32>(block.len);
		QByteArray l((const char *)&len, sizeof(len));
		data += l;

		data += vbf.blocks[i].data;

		uint16_t crc = crc16(block.data);
		crc = qToBigEndian<quint16>(crc);
		QByteArray c((const char *)&crc, sizeof(crc));
		data += c;
	}
	uint32_t crc = crc32(data);
	//qDebug() << "data crc32:" << hex << crc << vbf.header.file_checksum;
	vbf.header.file_checksum = crc;

	QByteArray header;

	header += "vbf_version = 2.1;\r\n";
	header += "header {\r\n";

	header += "    sw_part_number = \"" + vbf.header.sw_part_number + "\";\r\n";
	header += "    sw_part_type = " + vbf.header.sw_part_type + ";\r\n";
	header += "    network = " + vbf.header.network + ";\r\n";
	header += "    can_frame_format = " + vbf.header.can_frame_format + ";\r\n";
	header += "    ecu_address = " + QString("0x%1").arg(vbf.header.ecu_address, 4, 16, QChar('0')) + ";\r\n";

	if (vbf.header.sw_part_type == "SBL")
		header += "    call = " + QString("0x%1").arg(vbf.header.call, 4, 16, QChar('0')) + ";\r\n";

	if (vbf.header.erases.size()) {

		header += "    erase = {\r\n";

		for (int i = 0; i < vbf.header.erases.size(); i++) {

			const erase_t & erase = vbf.header.erases[i];
			header += QString("    { 0x%1, 0x%2 }\r\n").arg(erase.addr, 8, 16, QChar('0')). arg(erase.size, 8, 16, QChar('0'));
		}
		header += "    };\r\n";
	}

	header += "    file_checksum = " + QString("0x%1").arg(vbf.header.file_checksum, 8, 16, QChar('0')) + ";\r\n";

	header += "}";

	vbf.header.data = header;
}

