#include <QFile>
#include <QtEndian>
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>

#include "vbffile.h"
#include "lzss.h"

//CRC-32 normal 0x04c11db7 or reverse 0xedb88320
static const uint32_t crc32_poly = 0xedb88320;
static uint32_t crc32_table[256];
static uint32_t crc32_init(void)
{
	for (int i = 0; i < 256; ++i) {

		uint32_t cr = i;
		for (int j = 8; j > 0; --j)
			cr = cr & 0x00000001 ? (cr >> 1) ^ crc32_poly : (cr >> 1);
		crc32_table[i] = cr;
	}

	return ~0U;
}

static uint32_t crc32_calc(uint32_t crc, const QByteArray & data)
{
	for (int32_t i = 0; i < data.size(); i++)
		crc = (crc >> 8) ^ crc32_table[( crc ^ (data[i]) ) & 0xff];

	return crc;
}

static uint32_t crc32_finit(uint32_t crc)
{
	return ~crc;
}

static uint32_t crc32(const QByteArray & data)
{
	uint32_t crc = crc32_init();

	crc = crc32_calc(crc, data);

	return crc32_finit(crc);
}

//CRC-16/CCITT-FALSE
static const uint16_t crc16_poly = 0x1021;
static uint16_t crc16_init(void)
{
	return 0xffff;
}

static uint16_t crc16_calc(uint16_t crc, const QByteArray & data)
{
	for (int32_t i = 0; i < data.size(); i++) {

		crc ^= data[i] << 8;
		for (uint8_t j = 0; j < 8; j++) {

			if (crc & 0x8000)
				crc = (crc << 1) ^ crc16_poly;
			else
				crc = crc << 1;
		}
		crc &= 0xffff;
	}

    return crc;
}

static uint16_t crc16(const QByteArray & data)
{
	uint16_t crc = crc16_init();

	crc = crc16_calc(crc, data);

	return crc;
}

#if QT_VERSION < 0x050700
template <typename T> inline T qFromUnaligned(const void *src)
{
    T dest;
    const size_t size = sizeof(T);
    memcpy(&dest, src, size);
    return dest;
}


template <class T> inline T qFromBigEndian(const void *src)
{
    return qFromBigEndian(qFromUnaligned<T>(src));
}
#endif

bool vbf_open(const QString & fileName, vbf_t & vbf)
{
	qInfo() << "Opening file " << fileName << " ... ";

	QFile infile(fileName);
	if (!infile.open(QIODevice::ReadOnly)) {
		qWarning() << "Can't open file " << fileName;
		return false;
	}
	qDebug() << "file size:" << infile.size();

	QByteArray h = infile.read(sizeof("vbf_version = 99.99;"));
	int offset = h.indexOf("vbf_version");
	if (offset == -1) {
		qWarning() << "can't find header 'vbf_version' in file";
		infile.close();
		return false;
	}

	infile.seek(0);
	h = infile.read(HEADER_LIMIT_SIZE);

	//looking for such template: header { }
	offset = h.indexOf("header {");
	if (offset == -1) {
		qWarning() << "can't find begin of header";
		infile.close();
		return false;
	}
	offset += sizeof"header {";
	size_t left_braces = 1;
	size_t right_braces = 0;
	while (offset < h.size()) {
		if (h[offset] == '}')
			right_braces++;
		if (h[offset] == '{')
			left_braces++;
		if (left_braces == right_braces)
			break;
		offset++;
	}
	//qDebug().nospace() << "offset: 0x" << hex << offset << " left_braces:" << left_braces << " right_braces:" << right_braces;

	if (left_braces != right_braces) {
		qWarning() << "can't find end of header";
		infile.close();
		return false;
	}

	header_t header;
	header.file_checksum_offset = h.indexOf("0x", offset + 13);
	offset = h.indexOf("}", offset);

	offset = h.indexOf("}", offset);
	if (offset != -1)
		offset += 1;

	header.data = h.left(offset);
	qDebug() << "header:" << header.data;

	//parsing header
	QBuffer qbuf(&header.data);
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
		line.replace("=", " = ");

		QStringList list = line.split(QRegExp("[\r\n\t\",{} ]+"), QString::SkipEmptyParts);

		qDebug() << line;
		line.clear();
		qDebug() << list;

		//qDebug() << line << list.size();
		if (list.size() < 3)
			continue;

		if (list[1] != QString('='))
			continue;

		if (list[0] == "vbf_version")
			header.version = list[2];

		if (list[0] == "sw_part_number")
			header.sw_part_number = list[2];

		if (list[0] == "sw_part_type")
			header.sw_part_type = list[2];

		if (list[0] == "can_frame_format")
			header.can_frame_format = list[2];

		//vbf_version 2.2 and above
		if (list[0] == "frame_format") {

			header.can_frame_format = list[2];
			header.frame_format = true;
		}

		if (list[0] == "network")
			header.network = list[2];

		bool ok;
		if (list[0] == "erase") {

			list.removeAt(0);
			list.removeAt(0);
			//qDebug() << list;

			for (int i = 0; i < list.size()/2; i++) {

				erase_t erase;
				erase.addr = list[2 * i].toLongLong(&ok, 16);
				erase.size = list[2 * i + 1].toLongLong(&ok, 16);
				header.erases.push_back(erase);
			}
		}

		if (list[0] == "ecu_address")
			header.ecu_address = list[2].toLongLong(&ok, 16);

		if (list[0] == "call")
			header.call = list[2].toLongLong(&ok, 16);

		if (list[0] == "file_checksum")
			header.file_checksum = list[2].toLongLong(&ok, 16);

		if (list[0] == "data_format_identifier") {

			header.data_format_identifier_exist = true;
			header.data_format_identifier = list[2].toLongLong(&ok, 16);
		}
	}
	qbuf.close();

	vbf.filename = fileName;
	vbf.header = header;
	vbf.size = 0;

	//read all blocks
	uint32_t crc32 = crc32_init();
	infile.seek(offset);
	while (1) {

		block_t block;

		char _addr[4];
		if (4 != infile.read(_addr, 4))
			break;
		crc32 = crc32_calc(crc32, QByteArray(_addr, 4));
		block.addr = qFromBigEndian<quint32>(_addr);

		char _len[4];
		if (4 != infile.read(_len, 4))
			break;
		crc32 = crc32_calc(crc32, QByteArray(_len, 4));
		block.len = qFromBigEndian<quint32>(_len);

		qDebug().nospace() << "found block addr:0x" << hex << block.addr << " with size:0x" << hex << block.len << ", loading ...";
		block.data.reserve((block.len < BLOCK_LIMIT_SIZE) ? block.len : BLOCK_LIMIT_SIZE);

		block.offset = infile.pos();
		uint16_t crc16 = crc16_init();
		size_t nums = block.len/CHUNK_SIZE;
		nums = block.len % CHUNK_SIZE ? (nums + 1) : nums;
		for (size_t i = 0; i < nums; i++) {

			size_t sz = CHUNK_SIZE;
			if (i == (nums - 1))
				sz = block.len % CHUNK_SIZE;

			QByteArray chunk = infile.read(sz);
			QCoreApplication::processEvents();

			crc16 = crc16_calc(crc16, chunk);
			crc32 = crc32_calc(crc32, chunk);
			if (block.data.size() < BLOCK_LIMIT_SIZE)
				block.data += chunk;
		}

		if (block.len > BLOCK_LIMIT_SIZE)
			qWarning() << "Only first 1GB will be loaded";

		if (vbf.header.data_format_identifier_exist && vbf.header.data_format_identifier == 0x10) {

			QByteArray udata = decode(block.data);
			block.data = udata;
			qDebug() << "uncompress block data: " << block.len << " to "<< udata.size();
			crc16 = crc16_init();
			crc16 = crc16_calc(crc16, block.data);
		}

		char _crc[2];
		if (2 != infile.read(_crc, 2))
			break;
		crc32 = crc32_calc(crc32, QByteArray(_crc, 2));
		uint16_t _crc16 = qFromBigEndian<quint16>(_crc);

		qDebug().nospace() << "block addr: 0x" << hex << block.addr << " len: 0x" << block.len << " data: 0x" << block.data.size() << " _crc16: 0x" << _crc16 << " crc16: 0x" << crc16;
		if (_crc16 == crc16) {

			vbf.blocks.push_back(block);
			vbf.size += block.data.size();
		}
		else {

			qWarning() << "mismatch crc16:" << hex << crc16 << "block crc:" << hex << _crc16;
		}
	}

	infile.close();

	crc32 = crc32_finit(crc32);

	if (crc32 != header.file_checksum) {

		qWarning() << "mismatch crc32:" << hex << crc32 << "header crc:" << hex << header.file_checksum;
		return false;
	}
	qInfo() << "crc32 is OK";

	qInfo() << "vbf with " << vbf.blocks.size() << " block(s) successfully loaded";

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

bool vbf_insert(int idx, const QString & fileName, vbf_t & vbf)
{
	if (idx > vbf.blocks.size())
		return false;

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	block_t block;
	block.addr = 0;
	block.data = file.readAll();
	block.len = block.data.size();

	file.close();

	vbf.blocks.insert(idx, block);

	return true;
}

bool vbf_export_block(int idx, const QString & fileName, const vbf_t & vbf)
{
	if (idx > vbf.blocks.size())
		return false;

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
		return false;

	const block_t & block = vbf.blocks[idx];

	if (block.len > BLOCK_LIMIT_SIZE) {

		qWarning() << "Trying export block from source file " << vbf.filename;

		QFile infile(vbf.filename);
		if (!infile.open(QIODevice::ReadOnly))
			return false;

		infile.seek(block.offset);
		size_t nums = block.len/CHUNK_SIZE;
		nums = block.len % CHUNK_SIZE ? (nums + 1) : nums;
		for (size_t i = 0; i < nums; i++) {

			size_t sz = CHUNK_SIZE;
			if (i == (nums - 1))
				sz = block.len % CHUNK_SIZE;
			QByteArray chunk = infile.read(sz);
			file.write(chunk);
		}
	}
	else
		file.write(block.data);

	file.close();

	return true;
}

void vbf_export(const vbf_t & vbf)
{
	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		QString filename = vbf.filename + "." + QString::number(i) + ".bin";

		vbf_export_block(i, filename, vbf);
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
	qInfo() << "Saving file " << fileName << " ... ";

	QFile outfile(fileName);
	if (!outfile.open(QIODevice::WriteOnly)) {
		qWarning() << "Can't open file " << fileName;
		return;
	}

	//write header
	outfile.write(vbf.header.data);

	uint32_t crc32 = crc32_init();

	//write data
	for (int32_t i = 0; i < vbf.blocks.size(); i++) {

		uint16_t crc16 = crc16_init();

		const block_t & block = vbf.blocks[i];

		qDebug() << "save block with size" << block.len;

		uint32_t addr = qToBigEndian<quint32>(block.addr);
		QByteArray a((const char *)&addr, sizeof(addr));
		outfile.write(a);
		crc32 = crc32_calc(crc32, a);

		if (vbf.header.data_format_identifier_exist && vbf.header.data_format_identifier == 0x10) {

			QByteArray cdata = encode(block.data);
			qDebug() << "compress block data: " << block.data.size() << " to "<< cdata.size();

			uint32_t len = qToBigEndian<quint32>(cdata.size());
			QByteArray l((const char *)&len, sizeof(len));
			outfile.write(l);
			crc32 = crc32_calc(crc32, l);

			outfile.write(cdata);
			crc16 = crc16_calc(crc16, cdata);
			crc32 = crc32_calc(crc32, cdata);
		}
		else {

			uint32_t len = qToBigEndian<quint32>(block.len);
			QByteArray l((const char *)&len, sizeof(len));
			outfile.write(l);
			crc32 = crc32_calc(crc32, l);

			outfile.write(block.data);
			crc16 = crc16_calc(crc16, block.data);
			crc32 = crc32_calc(crc32, block.data);
		}

		crc16 = qToBigEndian<quint16>(crc16);
		QByteArray c((const char *)&crc16, sizeof(crc16));
		outfile.write(c);
		crc32 = crc32_calc(crc32, c);
	}
	crc32 = crc32_finit(crc32);

	//rewrite updated header
	QByteArray header = vbf.header.data;
	//if (vbf.header.data_format_identifier_exist && vbf.header.data_format_identifier == 0x10) {

		QByteArray ba = QString("0x%1").arg(crc32, 8, 16, QChar('0')).toLatin1();
		header.replace(vbf.header.file_checksum_offset, ba.size(), ba);
	//}
	outfile.seek(0);
	outfile.write(header);

	qInfo() << "vbf with " << vbf.blocks.size() << " block(s) successfully saved";

	outfile.close();
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

	QString version = vbf.header.version.isEmpty() ? "2.1" : vbf.header.version;
	header += "vbf_version = " + version.toLatin1() + ";\r\n";
	header += "header {\r\n";

	header += "    sw_part_number = \"" + vbf.header.sw_part_number.toLatin1() + "\";\r\n";
	header += "    sw_part_type = " + vbf.header.sw_part_type.toLatin1() + ";\r\n";
	if (vbf.header.data_format_identifier_exist)
		header += "    data_format_identifier = " + QString("0x%1").arg(vbf.header.data_format_identifier, 2, 16, QChar('0')).toLatin1() + ";\r\n";
	header += "    network = " + vbf.header.network.toLatin1() + ";\r\n";
	if (vbf.header.frame_format)
		header += "    frame_format = " + vbf.header.can_frame_format.toLatin1() + ";\r\n";
	else
		header += "    can_frame_format = " + vbf.header.can_frame_format.toLatin1() + ";\r\n";
	header += "    ecu_address = " + QString("0x%1").arg(vbf.header.ecu_address, 4, 16, QChar('0')).toLatin1() + ";\r\n";

	if (vbf.header.sw_part_type == "SBL")
		header += "    call = " + QString("0x%1").arg(vbf.header.call, 4, 16, QChar('0')).toLatin1() + ";\r\n";

	if (vbf.header.erases.size()) {

		header += "    erase = {\n";

		for (int i = 0; i < vbf.header.erases.size(); i++) {

			const erase_t & erase = vbf.header.erases[i];
			header += QString("    { 0x%1, 0x%2 }").arg(erase.addr, 8, 16, QChar('0')). arg(erase.size, 8, 16, QChar('0')).toLatin1();
			if (i != (vbf.header.erases.size() - 1))
				header += ",";

			header += "\n";
		}
		header += "    }; \r\n";
	}

	vbf.header.file_checksum_offset = header.size() + 20;
	header += "    file_checksum = " + QString("0x%1").arg(vbf.header.file_checksum, 8, 16, QChar('0')).toLatin1() + ";\r\n";

	header += "}";

	vbf.header.data = header;
}

