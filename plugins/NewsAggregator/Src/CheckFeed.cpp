/*
Copyright (C) 2012 Mataes

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

LPCTSTR CheckFeed(wchar_t *tszURL, CFeedEditor *pEditDlg)
{
	Netlib_LogfW(hNetlibUser, L"Started validating feed %s.", tszURL);
	char *szData = nullptr;
	GetNewsData(tszURL, &szData, NULL, pEditDlg);
	if (szData) {
		wchar_t *tszData = mir_utf8decodeW(szData);
		if (!tszData)
			tszData = mir_a2u(szData);
		int bytesParsed = 0;
		HXML hXml = xmlParseString(tszData, &bytesParsed, nullptr);
		mir_free(tszData);
		mir_free(szData);
		if (hXml != nullptr) {
			LPCTSTR codepage = nullptr;
			int childcount = 0;
			HXML node;
			HXML tmpnode = xmlGetFirstChild(hXml);
			if (!mir_wstrcmpi(xmlGetName(tmpnode), L"xml")) {
				int attrcount = xmlGetAttrCount(tmpnode);
				for (int i = 0; i < attrcount; i++) {
					LPCTSTR szAttrName = xmlGetAttrName(tmpnode, i);
					if (!mir_wstrcmpi(szAttrName, L"encoding")) {
						codepage = xmlGetAttrValue(tmpnode, szAttrName);
						break;
					}
				}
				node = xmlGetChild(tmpnode, childcount);
			}
			else if (!mir_wstrcmpi(xmlGetName(hXml), L"xml")) {
				int attrcount = xmlGetAttrCount(hXml);
				for (int i = 0; i < attrcount; i++) {
					LPCTSTR szAttrName = xmlGetAttrName(hXml, i);
					if (!mir_wstrcmpi(szAttrName, L"encoding")) {
						codepage = xmlGetAttrValue(hXml, szAttrName);
						break;
					}
				}
				node = xmlGetChild(hXml, childcount);
			}
			else
				node = hXml;
			while (node) {
				LPCTSTR szNodeName = xmlGetName(node);
				if (!mir_wstrcmpi(szNodeName, L"rss") || !mir_wstrcmpi(szNodeName, L"rdf")) {
					HXML chan = xmlGetChild(node, 0);
					for (int j = 0; j < xmlGetChildCount(chan); j++) {
						HXML child = xmlGetChild(chan, j);
						if (!mir_wstrcmpi(xmlGetName(child), L"title")) {
							wchar_t mes[MAX_PATH];
							mir_snwprintf(mes, TranslateT("%s\nis a valid feed's address."), tszURL);
							MessageBox(pEditDlg->GetHwnd(), mes, TranslateT("News Aggregator"), MB_OK | MB_ICONINFORMATION);
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								return mir_wstrdup(buf);
							}
							else
								return mir_wstrdup(xmlGetText(child));
						}
					}
				}
				else if (!mir_wstrcmpi(szNodeName, L"feed")) {
					for (int j = 0; j < xmlGetChildCount(node); j++) {
						HXML child = xmlGetChild(node, j);
						if (!mir_wstrcmpi(xmlGetName(child), L"title")) {
							wchar_t mes[MAX_PATH];
							mir_snwprintf(mes, TranslateT("%s\nis a valid feed's address."), tszURL);
							MessageBox(pEditDlg->GetHwnd(), mes, TranslateT("News Aggregator"), MB_OK | MB_ICONINFORMATION);
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								return mir_wstrdup(buf);
							}
							else
								return mir_wstrdup(xmlGetText(child));
						}
					}
				}
				node = xmlGetChild(hXml, ++childcount);
			}
		}
		xmlDestroyNode(hXml);
	}
	Netlib_LogfW(hNetlibUser, L"%s is not a valid feed's address.", tszURL);
	wchar_t mes[MAX_PATH];
	mir_snwprintf(mes, TranslateT("%s\nis not a valid feed's address."), tszURL);
	MessageBox(pEditDlg->GetHwnd(), mes, TranslateT("News Aggregator"), MB_OK | MB_ICONERROR);
	return nullptr;
}

static void XmlToMsg(MCONTACT hContact, CMStringW &title, CMStringW &link, CMStringW &descr, CMStringW &author, CMStringW &comments, CMStringW &guid, CMStringW &category, time_t stamp)
{
	CMStringW message = g_plugin.getWStringA(hContact, "MsgFormat");
	if (!message)
		message = TAGSDEFAULT;

	if (title.IsEmpty())
		message.Replace(L"#<title>#", TranslateT("empty"));
	else
		message.Replace(L"#<title>#", title);

	if (link.IsEmpty())
		message.Replace(L"#<link>#", TranslateT("empty"));
	else
		message.Replace(L"#<link>#", link);

	if (descr.IsEmpty())
		message.Replace(L"#<description>#", TranslateT("empty"));
	else
		message.Replace(L"#<description>#", descr);

	if (author.IsEmpty())
		message.Replace(L"#<author>#", TranslateT("empty"));
	else
		message.Replace(L"#<author>#", author);

	if (comments.IsEmpty())
		message.Replace(L"#<comments>#", TranslateT("empty"));
	else
		message.Replace(L"#<comments>#", comments);

	if (guid.IsEmpty())
		message.Replace(L"#<guid>#", TranslateT("empty"));
	else
		message.Replace(L"#<guid>#", guid);

	if (category.IsEmpty())
		message.Replace(L"#<category>#", TranslateT("empty"));
	else
		message.Replace(L"#<category>#", category);

	DBEVENTINFO olddbei = {};
	bool  MesExist = false;
	T2Utf pszTemp(message);
	DWORD cbMemoLen = 10000, cbOrigLen = (DWORD)mir_strlen(pszTemp);
	BYTE *pbBuffer = (BYTE*)mir_alloc(cbMemoLen);
	for (MEVENT hDbEvent = db_event_last(hContact); hDbEvent; hDbEvent = db_event_prev(hContact, hDbEvent)) {
		olddbei.cbBlob = db_event_getBlobSize(hDbEvent);
		if (olddbei.cbBlob > cbMemoLen)
			pbBuffer = (PBYTE)mir_realloc(pbBuffer, (size_t)(cbMemoLen = olddbei.cbBlob));
		olddbei.pBlob = pbBuffer;
		db_event_get(hDbEvent, &olddbei);

		// there's no need to look for the elder events
		if (stamp > 0 && olddbei.timestamp < (DWORD)stamp)
			break;

		if ((DWORD)mir_strlen((char*)olddbei.pBlob) == cbOrigLen && !mir_strcmp((char*)olddbei.pBlob, pszTemp)) {
			MesExist = true;
			break;
		}
	}
	mir_free(pbBuffer);

	if (!MesExist) {
		if (stamp == 0)
			stamp = time(0);

		T2Utf pszMessage(message);

		PROTORECVEVENT recv = { 0 };
		recv.timestamp = (DWORD)stamp;
		recv.szMessage = pszMessage;
		ProtoChainRecvMsg(hContact, &recv);
	}
}

void CheckCurrentFeed(MCONTACT hContact)
{
	// Check is disabled by the user?
	if (!g_plugin.getByte(hContact, "CheckState", 1) != 0)
		return;

	wchar_t *szURL = g_plugin.getWStringA(hContact, "URL");
	if (szURL == nullptr)
		return;

	Netlib_LogfW(hNetlibUser, L"Started checking feed %s.", szURL);

	char *szData = nullptr;
	GetNewsData(szURL, &szData, hContact, nullptr);
	mir_free(szURL);

	if (szData) {
		wchar_t *tszData = mir_utf8decodeW(szData);
		if (!tszData)
			tszData = mir_a2u(szData);
		int bytesParsed = 0;
		HXML hXml = xmlParseString(tszData, &bytesParsed, nullptr);
		mir_free(tszData);
		mir_free(szData);

		CMStringW szValue;
		if (hXml != nullptr) {
			LPCTSTR codepage = nullptr;
			int childcount = 0;
			HXML node;
			HXML tmpnode = xmlGetFirstChild(hXml);
			if (!mir_wstrcmpi(xmlGetName(tmpnode), L"xml")) {
				int attrcount = xmlGetAttrCount(tmpnode);
				for (int i = 0; i < attrcount; i++) {
					LPCTSTR szAttrName = xmlGetAttrName(tmpnode, i);
					if (!mir_wstrcmpi(szAttrName, L"encoding")) {
						codepage = xmlGetAttrValue(tmpnode, szAttrName);
						break;
					}
				}
				node = xmlGetChild(tmpnode, childcount);
			}
			else if (!mir_wstrcmpi(xmlGetName(hXml), L"xml")) {
				int attrcount = xmlGetAttrCount(hXml);
				for (int i = 0; i < attrcount; i++) {
					LPCTSTR szAttrName = xmlGetAttrName(hXml, i);
					if (!mir_wstrcmpi(szAttrName, L"encoding")) {
						codepage = xmlGetAttrValue(hXml, szAttrName);
						break;
					}
				}
				node = xmlGetChild(hXml, childcount);
			}
			else
				node = hXml;
			while (node) {
				LPCTSTR szNodeName = xmlGetName(node);
				bool isRSS = !mir_wstrcmpi(szNodeName, L"rss"), isAtom = !mir_wstrcmpi(szNodeName, L"rdf");
				if (isRSS || isAtom) {
					if (isRSS) {
						for (int i = 0; i < xmlGetAttrCount(node); i++) {
							LPCTSTR szAttrName = xmlGetAttrName(node, i);
							if (!mir_wstrcmpi(szAttrName, L"version")) {
								wchar_t ver[MAX_PATH];
								mir_snwprintf(ver, L"RSS %s", xmlGetAttrValue(node, szAttrName));
								g_plugin.setWString(hContact, "MirVer", ver);
								break;
							}
						}
					}
					else if (isAtom)
						g_plugin.setWString(hContact, "MirVer", L"RSS 1.0");

					HXML chan = xmlGetChild(node, 0);
					for (int j = 0; j < xmlGetChildCount(chan); j++) {
						HXML child = xmlGetChild(chan, j);
						LPCTSTR childName = xmlGetName(child);
						if (!mir_wstrcmpi(childName, L"title")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "FirstName", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(childName, L"link")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "Homepage", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(childName, L"description")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText) {
								ClearText(szValue, szChildText);
								g_plugin.setWString(hContact, "About", szValue);
								db_set_ws(hContact, "CList", "StatusMsg", szValue);
							}
						}
						else if (!mir_wstrcmpi(childName, L"language")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "Language1", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(childName, L"managingEditor")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "e-mail", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(childName, L"category")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "Interest0Text", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(childName, L"copyright")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText)
								db_set_s(hContact, "UserInfo", "MyNotes", _T2A(ClearText(szValue, szChildText)));
						}
						else if (!mir_wstrcmpi(childName, L"image")) {
							for (int x = 0; x < xmlGetChildCount(child); x++) {
								HXML imageval = xmlGetChild(child, x);
								if (!mir_wstrcmpi(xmlGetName(imageval), L"url")) {
									LPCTSTR url = xmlGetText(imageval);
									g_plugin.setWString(hContact, "ImageURL", url);

									PROTO_AVATAR_INFORMATION ai = { 0 };
									ai.hContact = hContact;

									ptrW szNick(g_plugin.getWStringA(hContact, "Nick"));
									if (szNick) {
										wchar_t *ext = wcsrchr((wchar_t *)url, '.') + 1;
										ai.format = ProtoGetAvatarFormat(url);

										CMStringW filename = szNick;
										filename.Replace(L"/", L"_");
										mir_snwprintf(ai.filename, L"%s\\%s.%s", tszRoot, filename.c_str(), ext);
										CreateDirectoryTreeW(tszRoot);
										if (DownloadFile(url, ai.filename)) {
											g_plugin.setWString(hContact, "ImagePath", ai.filename);
											ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, NULL);
										}
										else ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, NULL);
										break;
									}
								}
							}
						}
						else if (!mir_wstrcmpi(childName, L"lastBuildDate")) {
							LPCTSTR szChildText = nullptr;
							if (!mir_wstrcmpi(codepage, L"koi8-r")) {
								wchar_t buf[MAX_PATH];
								MultiByteToWideChar(20866, 0, _T2A(xmlGetText(child)), -1, buf, _countof(buf));
								szChildText = buf;
							}
							else
								szChildText = xmlGetText(child);
							if (szChildText) {
								time_t stamp = DateToUnixTime(szChildText, 0);
								double deltaupd = difftime(time(0), stamp);
								double deltacheck = difftime(time(0), (time_t)g_plugin.getDword(hContact, "LastCheck"));
								if (deltaupd - deltacheck >= 0) {
									g_plugin.setDword(hContact, "LastCheck", (DWORD)time(0));
									xmlDestroyNode(hXml);
									return;
								}
							}
						}
						else if (!mir_wstrcmpi(childName, L"item")) {
							CMStringW title, link, descr, author, comments, guid, category;
							time_t stamp = 0;
							for (int z = 0; z < xmlGetChildCount(child); z++) {
								HXML itemval = xmlGetChild(child, z);
								LPCTSTR itemName = xmlGetName(itemval);
								LPCTSTR value = nullptr;
								if (!mir_wstrcmpi(codepage, L"koi8-r")) {
									wchar_t buf[MAX_PATH];
									MultiByteToWideChar(20866, 0, _T2A(xmlGetText(itemval)), -1, buf, _countof(buf));
									value = buf;
								}
								else
									value = xmlGetText(itemval);

								// We only use the first tag for now and ignore the rest.
								if (!mir_wstrcmpi(itemName, L"title"))
									ClearText(title, value);

								else if (!mir_wstrcmpi(itemName, L"link"))
									ClearText(link, value);

								else if (!mir_wstrcmpi(itemName, L"pubDate") || !mir_wstrcmpi(itemName, L"date")) {
									if (stamp == 0)
										stamp = DateToUnixTime(value, 0);
								}
								else if (!mir_wstrcmpi(itemName, L"description") || !mir_wstrcmpi(itemName, L"encoded"))
									ClearText(descr, value);

								else if (!mir_wstrcmpi(itemName, L"author") || !mir_wstrcmpi(itemName, L"creator"))
									ClearText(author, value);

								else if (!mir_wstrcmpi(itemName, L"comments"))
									ClearText(comments, value);

								else if (!mir_wstrcmpi(itemName, L"guid"))
									ClearText(guid, value);

								else if (!mir_wstrcmpi(itemName, L"category"))
									ClearText(category, value);
							}

							XmlToMsg(hContact, title, link, descr, author, comments, guid, category, stamp);
						}
					}
				}
				else if (!mir_wstrcmpi(szNodeName, L"feed")) {
					g_plugin.setWString(hContact, "MirVer", L"Atom 3");
					for (int j = 0; j < xmlGetChildCount(node); j++) {
						HXML child = xmlGetChild(node, j);
						LPCTSTR szChildName = xmlGetName(child);
						if (!mir_wstrcmpi(szChildName, L"title")) {
							LPCTSTR szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "FirstName", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(szChildName, L"link")) {
							for (int x = 0; x < xmlGetAttrCount(child); x++) {
								if (!mir_wstrcmpi(xmlGetAttrName(child, x), L"rel"))
									if (!mir_wstrcmpi(xmlGetAttrValue(child, xmlGetAttrName(child, x)), L"self"))
										break;

								if (!mir_wstrcmpi(xmlGetAttrName(child, x), L"href"))
									g_plugin.setWString(hContact, "Homepage", xmlGetAttrValue(child, xmlGetAttrName(child, x)));
							}
						}
						else if (!mir_wstrcmpi(szChildName, L"subtitle")) {
							LPCTSTR szChildText = xmlGetText(child);
							if (szChildText) {
								ClearText(szValue, szChildText);
								g_plugin.setWString(hContact, "About", szValue);
								db_set_ws(hContact, "CList", "StatusMsg", szValue);
							}
						}
						else if (!mir_wstrcmpi(szChildName, L"language")) {
							LPCTSTR szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "Language1", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(szChildName, L"author")) {
							for (int x = 0; x < xmlGetChildCount(child); x++) {
								HXML authorval = xmlGetChild(child, x);
								if (!mir_wstrcmpi(xmlGetName(authorval), L"email")) {
									g_plugin.setWString(hContact, "e-mail", xmlGetText(authorval));
									break;
								}
							}
						}
						else if (!mir_wstrcmpi(szChildName, L"category")) {
							LPCTSTR szChildText = xmlGetText(child);
							if (szChildText)
								g_plugin.setWString(hContact, "Interest0Text", ClearText(szValue, szChildText));
						}
						else if (!mir_wstrcmpi(szChildName, L"icon")) {
							for (int x = 0; x < xmlGetChildCount(child); x++) {
								HXML imageval = xmlGetChild(child, x);
								if (!mir_wstrcmpi(xmlGetName(imageval), L"url")) {
									LPCTSTR url = xmlGetText(imageval);
									g_plugin.setWString(hContact, "ImageURL", url);

									ptrW szNick(g_plugin.getWStringA(hContact, "Nick"));
									if (szNick) {
										PROTO_AVATAR_INFORMATION ai = { 0 };
										ai.hContact = hContact;
										wchar_t *ext = wcsrchr((wchar_t *)url, '.') + 1;
										ai.format = ProtoGetAvatarFormat(ext);

										wchar_t *filename = szNick;
										mir_snwprintf(ai.filename, L"%s\\%s.%s", tszRoot, filename, ext);
										if (DownloadFile(url, ai.filename)) {
											g_plugin.setWString(hContact, "ImagePath", ai.filename);
											ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, NULL);
										}
										else ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, NULL);
										break;
									}
								}
							}
						}
						else if (!mir_wstrcmpi(szChildName, L"updated")) {
							LPCTSTR szChildText = xmlGetText(child);
							if (szChildText) {
								wchar_t *lastupdtime = (wchar_t *)szChildText;
								time_t stamp = DateToUnixTime(lastupdtime, 1);
								double deltaupd = difftime(time(0), stamp);
								double deltacheck = difftime(time(0), (time_t)g_plugin.getDword(hContact, "LastCheck"));
								if (deltaupd - deltacheck >= 0) {
									g_plugin.setDword(hContact, "LastCheck", (DWORD)time(0));
									xmlDestroyNode(hXml);
									return;
								}
							}
						}
						else if (!mir_wstrcmpi(szChildName, L"entry")) {
							CMStringW title, link, descr, author, comments, guid, category;
							time_t stamp = 0;
							for (int z = 0; z < xmlGetChildCount(child); z++) {
								HXML itemval = xmlGetChild(child, z);
								LPCTSTR szItemName = xmlGetName(itemval);
								if (!mir_wstrcmpi(szItemName, L"title")) {
									LPCTSTR szItemText = xmlGetText(itemval);
									if (szItemText)
										ClearText(title, szItemText);
								}
								else if (!mir_wstrcmpi(szItemName, L"link")) {
									for (int x = 0; x < xmlGetAttrCount(itemval); x++) {
										if (!mir_wstrcmpi(xmlGetAttrName(itemval, x), L"href")) {
											ClearText(link, xmlGetAttrValue(itemval, xmlGetAttrName(itemval, x)));
											break;
										}
									}
								}
								else if (!mir_wstrcmpi(szItemName, L"updated")) {
									if (stamp == 0)
										stamp = DateToUnixTime(xmlGetText(itemval), 0);
								}
								else if (!mir_wstrcmpi(szItemName, L"summary") || !mir_wstrcmpi(szItemName, L"content")) {
									LPCTSTR szItemText = xmlGetText(itemval);
									if (szItemText)
										ClearText(descr, szItemText);
								}
								else if (!mir_wstrcmpi(szItemName, L"author")) {
									for (int x = 0; x < xmlGetChildCount(itemval); x++) {
										HXML authorval = xmlGetChild(itemval, x);
										if (!mir_wstrcmpi(xmlGetName(authorval), L"name") && xmlGetText(authorval)) {
											ClearText(author, xmlGetText(authorval));
											break;
										}
									}
								}
								else if (!mir_wstrcmpi(szItemName, L"comments")) {
									LPCTSTR szItemText = xmlGetText(itemval);
									if (szItemText)
										ClearText(comments, szItemText);
								}
								else if (!mir_wstrcmpi(szItemName, L"id")) {
									LPCTSTR szItemText = xmlGetText(itemval);
									if (szItemText)
										ClearText(guid, xmlGetText(itemval));
								}
								else if (!mir_wstrcmpi(szItemName, L"category")) {
									for (int x = 0; x < xmlGetAttrCount(itemval); x++) {
										LPCTSTR szAttrName = xmlGetAttrName(itemval, x);
										if (!mir_wstrcmpi(szAttrName, L"term") && xmlGetText(itemval)) {
											ClearText(category, xmlGetAttrValue(itemval, szAttrName));
											break;
										}
									}
								}
							}

							XmlToMsg(hContact, title, link, descr, author, comments, guid, category, stamp);
						}
					}
				}
				node = xmlGetChild(hXml, ++childcount);
			}
			xmlDestroyNode(hXml);
		}
	}
	g_plugin.setDword(hContact, "LastCheck", (DWORD)time(0));
}

void CheckCurrentFeedAvatar(MCONTACT hContact)
{
	if (!g_plugin.getByte(hContact, "CheckState", 1))
		return;

	wchar_t *szURL = g_plugin.getWStringA(hContact, "URL");
	if (szURL == nullptr)
		return;

	char *szData = nullptr;
	GetNewsData(szURL, &szData, hContact, nullptr);
	mir_free(szURL);

	if (szData == nullptr)
		return;

	wchar_t *tszData = mir_utf8decodeW(szData);
	if (!tszData)
		tszData = mir_a2u(szData);
	int bytesParsed = 0;
	HXML hXml = xmlParseString(tszData, &bytesParsed, nullptr);
	mir_free(tszData);
	mir_free(szData);
	if (hXml == nullptr)
		return;

	int childcount = 0;
	HXML node = xmlGetChild(hXml, childcount);
	while (node) {
		LPCTSTR szNodeName = xmlGetName(node);
		if (!mir_wstrcmpi(szNodeName, L"rss") || !mir_wstrcmpi(szNodeName, L"rdf")) {
			HXML chan = xmlGetChild(node, 0);
			for (int j = 0; j < xmlGetChildCount(chan); j++) {
				HXML child = xmlGetChild(chan, j);
				if (!mir_wstrcmpi(xmlGetName(child), L"image")) {
					for (int x = 0; x < xmlGetChildCount(child); x++) {
						HXML imageval = xmlGetChild(child, x);
						if (!mir_wstrcmpi(xmlGetName(imageval), L"url")) {
							LPCTSTR url = xmlGetText(imageval);
							g_plugin.setWString(hContact, "ImageURL", url);

							PROTO_AVATAR_INFORMATION ai = { 0 };
							ai.hContact = hContact;

							ptrW szNick(g_plugin.getWStringA(hContact, "Nick"));
							if (szNick) {
								wchar_t *ext = wcsrchr((wchar_t *)url, '.') + 1;
								ai.format = ProtoGetAvatarFormat(ext);

								wchar_t *filename = szNick;
								mir_snwprintf(ai.filename, L"%s\\%s.%s", tszRoot, filename, ext);
								if (DownloadFile(url, ai.filename)) {
									g_plugin.setWString(hContact, "ImagePath", ai.filename);
									ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, NULL);
								}
								else ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, NULL);
								break;
							}
						}
					}
				}
			}
		}
		else if (!mir_wstrcmpi(szNodeName, L"feed")) {
			for (int j = 0; j < xmlGetChildCount(node); j++) {
				HXML child = xmlGetChild(node, j);
				if (!mir_wstrcmpi(xmlGetName(child), L"icon")) {
					for (int x = 0; x < xmlGetChildCount(child); x++) {
						HXML imageval = xmlGetChild(child, x);
						if (!mir_wstrcmpi(xmlGetName(imageval), L"url")) {
							LPCTSTR url = xmlGetText(imageval);
							g_plugin.setWString(hContact, "ImageURL", url);

							ptrW szNick(g_plugin.getWStringA(hContact, "Nick"));
							if (szNick) {
								PROTO_AVATAR_INFORMATION ai = { 0 };
								ai.hContact = hContact;

								wchar_t *ext = wcsrchr((wchar_t *)url, '.') + 1;
								ai.format = ProtoGetAvatarFormat(ext);

								wchar_t *filename = szNick;
								mir_snwprintf(ai.filename, L"%s\\%s.%s", tszRoot, filename, ext);
								if (DownloadFile(url, ai.filename)) {
									g_plugin.setWString(hContact, "ImagePath", ai.filename);
									ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, NULL);
								}
								else ProtoBroadcastAck(MODULENAME, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, NULL);
								break;
							}
						}
					}
				}
			}
		}
		node = xmlGetChild(hXml, ++childcount);
	}
	xmlDestroyNode(hXml);
}
