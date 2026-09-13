# PC decoded light: whole Scene attachment and world cache refresh

Source spLight now holds an explicit borrowed SceneLightManager binding.
Its world clears8 and invokes that manager before the derived DX payload.
spLightManager retains explicit borrowed cache/sphere targets and applies the
same eligibility/add/remove algorithm in stable order. Register/unregister
guards and vector storage are host choices. Binding is not automatic native
Node attachment, a complete source Scene class, partition traversal or scene
render initialization. The native manager+1C render-list link is the explicit
45D850 assignment; the unrelated full SceneInit is not executed. Renderer
storage is zero caller input inside the same arena, without OS/GPU forwarding.
