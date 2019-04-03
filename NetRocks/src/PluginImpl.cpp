#include "Globals.h"
#include <KeyFileHelper.h>
#include "PluginImpl.h"
#include "UI/SiteConnectionEditor.h"


PluginImpl::PluginImpl(const char *path)
{
	_cur_dir[0] = _panel_title[0] = 0;
}

PluginImpl::~PluginImpl()
{
}

void PluginImpl::UpdatePanelTitle()
{
	std::string tmp;
	if (_connection) {
		tmp = _connection->SiteInfo();
//		tmp+= '/';
		tmp+= _cur_dir;
	} else {
		tmp = "NetRocks connections list";
	}

	if (tmp.size() >= sizeof(_panel_title)) {
		size_t rmlen = 4 + (tmp.size() - sizeof(_panel_title));
		tmp.replace((tmp.size() - rmlen) / 2 , rmlen, "...");
	}
	
	memcpy(_panel_title, tmp.c_str(), tmp.size() + 1);
}

bool PluginImpl::ValidateConnection()
{
	try {
		if (!_connection)
			return false;

		if (_connection->IsBroken()) 
			throw std::runtime_error("Connection broken");

	} catch (std::exception &e) {
		fprintf(stderr, "NetRocks::ValidateConnection: %s\n", e.what());
		_connection.reset();
		return false;
	}

	return true;
}

int PluginImpl::GetFindData(PluginPanelItem **pPanelItem, int *pItemsNumber, int OpMode)
{
	fprintf(stderr, "NetRocks::GetFindData '%s' G.config='%s'\n", _cur_dir, G.config.c_str());
	FP_SizeItemList il(FALSE);
	try {
		if (!ValidateConnection()) {
			_cur_dir[0] = 0;
			KeyFileHelper kfh(G.config.c_str());
			const std::vector<std::string> &sites = kfh.EnumSections();
			for (const auto &site : sites) {
				PluginPanelItem tmp = {};
				strncpy(tmp.FindData.cFileName, site.c_str(), sizeof(tmp.FindData.cFileName));
				tmp.FindData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				if (!il.Add(&tmp))
					throw std::runtime_error("Can't add list entry");
			}

		} else {
			_connection->DirectoryEnum(_cur_dir, il, OpMode);
		}

	} catch (std::exception &e) {
		fprintf(stderr, "NetRocks::GetFindData('%s', %d) ERROR: %s\n", _cur_dir, OpMode, e.what());
		il.Free(il.Items(), il.Count() );
		*pPanelItem = nullptr;
		*pItemsNumber = 0;
		return FALSE;
	}

	*pPanelItem = il.Items();
	*pItemsNumber = il.Count();
	return TRUE;
}

void PluginImpl::FreeFindData(PluginPanelItem *PanelItem, int ItemsNumber)
{
	FP_SizeItemList::Free(PanelItem, ItemsNumber);
}

int PluginImpl::SetDirectory(const char *Dir, int OpMode)
{
	fprintf(stderr, "NetRocks::SetDirectory('%s', %d)\n", Dir, OpMode);
	if (strcmp(Dir, ".") == 0)
		return TRUE;

	try {
		std::string tmp = _cur_dir;

		if (strcmp(Dir, "..") == 0) {
			size_t p = tmp.rfind('/');
			if (p == std::string::npos)
				p = 0;
			tmp.resize(p);

		} else if (*Dir == '/') {
			tmp = Dir + 1;

		} else {
			if (!tmp.empty())
				tmp+= '/';
			tmp+= Dir;
		}
		while (!tmp.empty() && tmp[tmp.size() - 1] == '/')
			tmp.resize(tmp.size() - 1);

		size_t p = tmp.find('/');
		if (!_connection) {
			const std::string &site = tmp.substr(0, p);
			try {
				_connection.reset(new SiteConnection(site, OpMode));

			} catch (std::runtime_error &e) {
				fprintf(stderr,
					"NetRocks::SetDirectory: new SiteConnection('%s') - %s\n",
					site.c_str(), e.what());

				return FALSE;
			}

//			if (p != std::string::npos)
//				tmp.erase(0, p + 1);
//			else
//				tmp.clear();
		}

		if (p != std::string::npos && p < tmp.size() - 1) {
			mode_t mode = _connection->GetMode(tmp.substr(p + 1));
			if (!S_ISDIR(mode))
				throw std::runtime_error("Not a directory");
		}

		if (tmp.size() >= sizeof(_cur_dir))
			throw std::runtime_error("Too long path");

		memcpy(_cur_dir, tmp.c_str(), tmp.size() + 1);

	} catch (std::exception &e) {
		fprintf(stderr, "NetRocks::SetDirectory('%s', %d) ERROR: %s\n", Dir, OpMode, e.what());
		return FALSE;
	}

	fprintf(stderr, "NetRocks::SetDirectory('%s', %d) OK: '%s'\n", Dir, OpMode, _cur_dir);

	return TRUE;
}

void PluginImpl::GetOpenPluginInfo(struct OpenPluginInfo *Info)
{
	fprintf(stderr, "Inside::GetOpenPluginInfo: '%s' \n", _cur_dir);
//	snprintf(_panel_title, ARRAYSIZE(_panel_title),
//	          " Inside: %s@%s ", _dir.c_str(), _name.c_str());

	Info->Flags = OPIF_SHOWPRESERVECASE | OPIF_USEHIGHLIGHTING;
	Info->HostFile = NULL;
	Info->CurDir = _cur_dir;
	Info->PanelTitle = _panel_title;
}

int PluginImpl::GetFiles(struct PluginPanelItem *PanelItem, int ItemsNumber, int Move, char *DestPath, int OpMode)
{
	fprintf(stderr, "Inside::GetFiles: _dir='%s' DestPath='%s'\n", _cur_dir, DestPath);
	if (ItemsNumber == 0 || Move)
		return FALSE;

	BOOL out = TRUE;
	std::string data_path;
	for (int i = 0; i < ItemsNumber; ++i) {
		data_path = DestPath;
		if (!data_path.empty() && data_path[data_path.size() - 1] != '/') {
			data_path+= '/';
		}
		data_path+= PanelItem[i].FindData.cFileName;

		uint64_t len = PanelItem[i].FindData.nFileSizeHigh;
		len<<= 32;
		len|= PanelItem[i].FindData.nFileSizeLow;

		//bool rv = OnGetFile(PanelItem[i].FindData.cFileName, data_path.c_str(), len);
		//fprintf(stderr, "Inside::GetFiles[%i]: %s '%s'\n",
		//	i, rv ? "OK" : "ERROR", PanelItem[i].FindData.cFileName);
	}

	return out;
}


int PluginImpl::PutFiles(struct PluginPanelItem *PanelItem, int ItemsNumber, int Move, int OpMode)
{
	if (ItemsNumber == 0 || Move)
		return FALSE;

	BOOL out = TRUE;
	char cd[0x1000] = {};
	sdc_getcwd(cd, sizeof(cd) - 1);
	std::string data_path;
	for (int i = 0; i < ItemsNumber; ++i) {
		if (PanelItem[i].FindData.cFileName[0] != '/') {
			data_path = cd;
			data_path+= '/';
			data_path+= PanelItem[i].FindData.cFileName;
		} else
			data_path = PanelItem[i].FindData.cFileName;

		//bool rv = OnPutFile(PanelItem[i].FindData.cFileName, data_path.c_str());
		//fprintf(stderr, "Inside::PutFiles[%i]: %s '%s'\n",
		//	i, rv ? "OK" : "ERROR", PanelItem[i].FindData.cFileName);

		//if (!rv)
		//	out = FALSE;
	}

	return out;
}

int PluginImpl::DeleteFiles(struct PluginPanelItem *PanelItem, int ItemsNumber, int OpMode)
{
	if (ItemsNumber == 0 )
		return FALSE;

	for (int i = 0; i < ItemsNumber; ++i) {
		//bool rv = OnDeleteFile(PanelItem[i].FindData.cFileName);
		//fprintf(stderr, "Inside::DeleteFiles[%i]: %s '%s'\n",
		//	i, rv ? "OK" : "ERROR", PanelItem[i].FindData.cFileName);
	}

	return TRUE;
}

int PluginImpl::ProcessKey(int Key, unsigned int ControlState)
{
	fprintf(stderr, "Inside::ProcessKey\n");

	if (!_cur_dir[0] && Key==VK_F4
	&& (ControlState == 0 || ControlState == PKF_SHIFT))
	{
		std::string site;

		if (ControlState == 0) {
			PanelInfo pi = {};
			G.info.Control(this, FCTL_GETPANELINFO, &pi);

			if (pi.CurrentItem >= pi.ItemsNumber)
				return TRUE;

			site = pi.PanelItems[pi.CurrentItem].FindData.cFileName;
		}
		SiteConnectionEditor sce(site);
		if (sce.Edit()) {
			SetDirectory(sce.SiteName().c_str(), 0);
		}

		return TRUE;
	}

	return FALSE;
}