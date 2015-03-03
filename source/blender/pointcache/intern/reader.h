/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PTC_READER_H
#define PTC_READER_H

#include <string>

#include "util_error_handler.h"
#include "util_types.h"
#include "PTC_api.h"

struct ID;

namespace PTC {

class ReaderArchive {
public:
	virtual ~ReaderArchive() {}
	
	virtual bool get_frame_range(int &start_frame, int &end_frame) = 0;
	
	virtual PTCReadSampleResult test_sample(float frame) = 0;
};

class Reader {
public:
	Reader(ID *id, const std::string &name);
	virtual ~Reader();
	
	void set_archive(ReaderArchive *archive);
	
	void set_error_handler(ErrorHandler *handler);
	ErrorHandler *get_error_handler() const { return m_error_handler; }
	bool valid() const;
	
	inline bool get_frame_range(int &start_frame, int &end_frame)
	{
		return m_archive->get_frame_range(start_frame, end_frame);
	}
	
	inline PTCReadSampleResult test_sample(float frame)
	{
		return m_archive->test_sample(frame);
	}
	
	virtual PTCReadSampleResult read_sample(float frame) = 0;
	
	ID *id() const { return m_id; }
	const std::string &name() const { return m_name; }
	
protected:
	/* called after the archive is set */
	virtual void open_archive(ReaderArchive *archive) = 0;
	
protected:
	ErrorHandler *m_error_handler;
	ReaderArchive *m_archive;
	
	std::string m_name;
	ID *m_id;
};

} /* namespace PTC */

#endif  /* PTC_READER_H */
