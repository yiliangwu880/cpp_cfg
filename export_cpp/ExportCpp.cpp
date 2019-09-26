#include "ExportCpp.h"
#include <sstream>
#include <limits>



using json = nlohmann::json;
using namespace std;
using namespace SimpleCfgLog;

namespace
{
	string Js2String(json &js)
	{
		std::stringstream ss;
		ss << js;

		return ss.str();
	}
	//return -1表示失败，成功返回索引
	template<class T>
	int FindValueInArray(const json &js, const T &value)
	{
		if (!js.is_array())
		{
			return -1;
		}
		for (int i=0; i<js.size(); ++i)
		{
			if (js[i] == value)
			{
				return i;
			}
		}
		return -1;
	}

	const char * NumToStr(int num)
	{
		static char buf[100];
		snprintf(buf, sizeof(buf), "%d", num);
		return buf;
	}
}

bool JsToCpp::Build(const nlohmann::json &js, const std::string &file_name, std::string &cpp_str)
{
	L_DEBUG("JsToCpp::Build %s", js.dump().c_str());
	if (!IsEnableObj(js))
	{
		return false;
	}
	string class_name;
	CfgFileNamePrefix(file_name, class_name);

	{
		cpp_str += R"(
//this file is build by CppCfg Tool
//don't modify by manual
#pragma once
#include <string>
#include <array>
#include "SimpleCfg.h"
)";

		cpp_str += "\nstruct " + class_name + "\n";
		cpp_str += "{\n";

		string tab = "	";
		string str;
		BuildSubClassAndMember(js, str, tab);
		cpp_str += str;

	}

	{
		string str;
		BuildMethod(js, file_name, str);
		cpp_str += str;
	}
	cpp_str += "};\n";
	return true;
}

string JsToCpp::GetBaseCppType(const nlohmann::json &js)
{
	if (js.is_number_float())
	{
		return "double";
	}
	else if (js.is_number_unsigned())
	{
		int64 v = js.get<uint64>();
		if (v > std::numeric_limits<uint32>::max())
		{
			return "uint64";
		}
		if (v > std::numeric_limits<uint16>::max())
		{
			return "uint32";
		}
		return "uint16";
	}
	else if (js.is_number_integer())
	{
		int64 v = js.get<int64>();
		if (v > std::numeric_limits<int32>::max() || v < std::numeric_limits<int32>::min())
		{
			return "int64";
		}
		if (v > std::numeric_limits<int16>::max() || v < std::numeric_limits<int16>::min())
		{
			return "int32";
		}
		return "int16";
	}
	else if (js.is_number())
	{
		int64 v = js.get<int64>();
		if (v > std::numeric_limits<int32>::max() || v < std::numeric_limits<int32>::min())
		{
			return "int64";
		}
		if (v > std::numeric_limits<int16>::max() || v < std::numeric_limits<int16>::min())
		{
			return "int32";
		}
		return "int16";
	}
	else if (js.is_boolean())
	{
		return "bool";
	}
	else if (js.is_string())
	{
		return "std::string";
	}
	Log("error, unknow type");
	return "unknow";
}

std::string JsToCpp::GetArrayCppType(const nlohmann::json &js)
{
	if (!js.is_array())
	{
		Log("error, para is not array");
		return "ErrorArray";
	}
	//类型对应优先级
	static std::map<string, uint32> m_type_2_pri=
	{
		{"double", 1},
		{"int64", 2},
		{"uint64", 3},
		{"int32", 4},
		{"uint32", 5},
		{"int16", 6},
		{"uint16", 7},
	};
	std::map<uint32, string> m_type; //候选数字类型，最优先的放前面

	for (uint32 i=0; i<js.size(); ++i)
	{
		if (!js[i].is_primitive())
		{
			Log("error, array's elemt must be primitive");
			return "ErrorArray";
		}
		if (!js[i].is_number())
		{
			return GetBaseCppType(js[i]);
		}
		string s = GetBaseCppType(js[i]);
		auto it = m_type_2_pri.find(s);
		if (it == m_type_2_pri.end())
		{
			Log("error, unknow primitive type [%s]", s.c_str());
			return "ErrorArray";
		}
		m_type[it->second]=it->first;
	}

	return m_type.begin()->second;
}

void JsToCpp::CfgFileNamePrefix(const std::string &file_name, std::string &prefix)
{
	auto it = std::find(file_name.begin(), file_name.end(), '.');

	prefix.assign(file_name.begin(), it);
}

bool JsToCpp::BuildMethod(const nlohmann::json &js, const std::string &file_name, std::string &cpp_str)
{
	const char *START_STR1 = R"(
	////////////////////////method list////////////////////////
	//load or reload cfg file .
	bool LoadFile(const char *file_name=nullptr)
	{
		//default load original cfg file name
		if (nullptr == file_name)
		{
			file_name = ")"; 
	
const char *START_STR2 = R"(";
		}
		SimpleCfg js;
		if (!js.ParseFile(file_name))
		{
			return false;
		}
		return Assign(js);
	}

private:
	template<typename Array>
	inline size_t ArrayLen(const Array &array)
	{
		return sizeof(array) / sizeof(array[0]);
	}
	//initialize or reload cfg content.
	bool Assign(const nlohmann::json &js)
	{
		try
		{

)";
	const char *END_STR = R"(

			return true;
		}
		catch (...)
		{
			//if fail, pls check if your cfg fomart is legal.
			return false;
		}
	}
)";
	string assign_str;
	BuildAssignMem(js, assign_str, "", "js");

	cpp_str = START_STR1 + file_name + START_STR2 + assign_str + END_STR;
	return true;
}

bool JsToCpp::BuildAssignMem(const nlohmann::json &js, std::string &cpp_str, std::string preName, std::string preName_js)
{
	//build member assign
	const string TAB = "			";
	cpp_str.clear();
	const json *dynArray = nullptr;
	{
		auto it = js.find(SimpleCfg::DynamicStr);
		if (it != js.end())
		{
			dynArray = &js[SimpleCfg::DynamicStr];
		}
	}

	for (auto &mem : js.items())
	{
		if (mem.key() == SimpleCfg::DynamicStr)
		{
			continue;
		}
		//build dynamic obj
		if (nullptr != dynArray)
		{
			L_DEBUG("find key %s", mem.key().c_str());
			if (-1 != FindValueInArray(*dynArray, mem.key()))
			{
				cpp_str += TAB + preName + mem.key() + " = " + preName_js + "[\"" + mem.key() + "\"].dump().c_str();\n";
				continue;
			}
		}
		
		//build sub class
		if (mem.value().is_object())
		{
			string str;
			BuildAssignMem(mem.value(), str, preName + mem.key() + ".", preName_js + "[\""+ mem.key() +"\"]");
			cpp_str += str;
		}
		else if (mem.value().is_array())//数组不能二维
		{
			const json &arr = mem.value();
			for (uint32 i=0; i<arr.size(); ++i)
			{
				const json &el = arr[i];
				if (el.is_array())
				{
					Log("error, array can't mult");
					return false;
				}
				if (el.is_object())
				{
					string str;
					BuildAssignMem(el, str, preName + mem.key() + "[" + NumToStr(i) + "].", preName_js + "[\"" + mem.key() + "\"]"+"[" + NumToStr(i) + "]");
					cpp_str += str;
				}
				else
				{
					cpp_str += TAB + preName + mem.key() + "[" + NumToStr(i) + "]" + " = " + preName_js + "[\"" + mem.key() + "\"]"+"[" + NumToStr(i) + "]" + ";\n";
				}
			}
		}
		else
		{
			cpp_str += TAB + preName + mem.key() + " = " + preName_js+ "[\"" + mem.key() + "\"]" + ";\n";
		}
	}
	return  true;
}

void JsToCpp::BuildSubClassAndMember(const nlohmann::json &js, string &str, string &tab)
{
	if (!js.is_object())
	{
		Log("error, para is not object");
		return;
	}
	str.clear();
	string mem_list; //定义成员字符串
	const json *dynArray = nullptr;
	{
		auto it = js.find(SimpleCfg::DynamicStr);
		if (it != js.end())
		{
			dynArray = &js[SimpleCfg::DynamicStr];
		}
	}
	for (auto &mem : js.items())
	{
		if (mem.key() == SimpleCfg::DynamicStr)
		{
			continue;
		}
		//build dynamic obj
		if (nullptr != dynArray)
		{
			L_DEBUG("find key %s", mem.key().c_str());
			if (-1 != FindValueInArray(*dynArray, mem.key()))
			{
				L_DEBUG("build dynamic");
				mem_list += tab + "std::string" + string(" ") + mem.key() + ";\n";
				continue;
			}
		}

		//build sub class
		if (mem.value().is_object())
		{
			string class_name = "S_" + mem.key();

			str += tab + "struct " + class_name + "\n";
			str += tab + "{\n";

			string sub_class;
			string sub_tab = tab + "	";
			BuildSubClassAndMember(mem.value(), sub_class, sub_tab);
			str += sub_class;

			str += tab + "};\n";
			//build member
			mem_list += tab + class_name + " " +mem.key() + ";\n";
		}
		else if (mem.value().is_array())
		{
			const json &elment = mem.value()[0];
			if (elment.is_array())
			{
				Log("error, array can't Multi");
				return;
			}
			else if (elment.is_object())
			{
				string class_name = "S_" + mem.key();
				str += tab + "struct " + class_name + "\n";
				str += tab + "{\n";

				//build elemnt's member
				string sub_tab = tab + "	";
				string arr_str;
				BuildSubClassAndMember(elment, arr_str, sub_tab);
				str += arr_str;

				str += tab + "};\n";
				//build member
				mem_list += tab + "std::array<" + class_name + "," + NumToStr(mem.value().size()) + "> " + mem.key() + ";\n";
			}
			else
			{
				//build member
				mem_list += tab + "std::array<"+(GetArrayCppType(mem.value())) + ","+ NumToStr(mem.value().size())+"> " + mem.key() + ";\n";
			}
		}
		else
		{
			mem_list += tab + (GetBaseCppType(mem.value()) + string(" ") + mem.key() + ";\n");
		}
	}
	if (tab == "	")//第一层
	{
		str += "\n\n	////////////////////////define member list////////////////////////\n";
	}
	str += mem_list;
}

bool JsToCpp::IsEnableObj(const nlohmann::json &js)
{
	if (!js.is_object())
	{
		Log("error, para is not object");
		return false;
	}

	const json *dynArray = nullptr;
	{
		auto it = js.find(SimpleCfg::DynamicStr);
		if (it != js.end())
		{
			dynArray = &js[SimpleCfg::DynamicStr];
		}
	}

	for (auto &el : js.items())
	{
		const json &member = el.value();
		const string &key = el.key();
		if (nullptr != dynArray)
		{
			//for (auto& v : *dynArray)
			auto it = dynArray->find(key);
			if (it != dynArray->end())
			{
				continue; //忽略检查动态对象
			}
		}
	
		if (member.is_object())
		{
			if (!IsEnableObj(member))
			{
				return false;
			}
		}
		else if (member.is_array())
		{
			if (!IsEnableArray(member))
			{
				return false;
			}
		}
		//base member is always enable
	}
	return true;
}

bool JsToCpp::IsEnableArray(const nlohmann::json &js)
{
	if (!js.is_array())
	{
		Log("error para, js is not array");
		return false;
	}
	json js_1st;
	for (const json &member : js)
	{
		if (js_1st.is_null())
		{
			js_1st = member;
		}
		else
		{
			if (js_1st.type() != member.type())
			{
				if (!(js_1st.is_number() && member.is_number()))
				{
					Log("error , SimpleCfg array element must be same type");
					return false;
				}
			}
		}
	}
	return true;
}