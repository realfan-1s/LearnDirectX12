#include <Material.h>
#include <Texture.h>

MaterialData::MaterialData(std::string _name, UINT _matIndex, float _roughness, const XMFLOAT3& _emission, float _metalness, BlendType _type)
	: name(std::move(_name)), materialCBIndex(_matIndex), roughness(_roughness), emission(_emission), metalness(_metalness), type(_type)
{
}

void Material::CreateMaterial(const std::string& name, std::string_view texName, UINT materialCBIndex, float roughness, const XMFLOAT3& emission, float metalness, BlendType type)
{
	auto md = std::make_unique<MaterialData>(name, materialCBIndex, roughness, emission, metalness, type);
	md->diffuseIndex = TextureMgr::instance().GetRegisterType(texName).value();
	std::string normName(texName);
	normName.append("Norm");
	md->normalIndex = TextureMgr::instance().GetRegisterType(normName).value_or(0);
	m_data[name] = std::move(md);
}

void Material::CreateMaterial(std::unique_ptr<MaterialData> data)
{
	m_data[data->name] = std::move(data);
}

void Material::Update(const GameTimer& timer, UploaderBuffer<MaterialConstant>* currMatConstant)
{
	for (auto& [name, mat] : m_data)
	{
		// 只有材质常量数据发生变化才要更新帧资源
		if (mat->dirtyFlag > 0)
		{
			MaterialConstant matConstants;
			matConstants.emission = mat->emission;
			matConstants.roughness = mat->roughness;
			matConstants.metalness = mat->metalness;
			matConstants.diffuseIndex = mat->diffuseIndex;
			matConstants.normalIndex = mat->normalIndex;
			currMatConstant->Copy(mat->materialCBIndex, matConstants);
			// 对下一个FrameResource进行更新
			mat->dirtyFlag--;
		}
	}
}
