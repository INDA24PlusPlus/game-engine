## Steps:
1. Download assets from [OneDrive](https://kth-my.sharepoint.com/:f:/g/personal/oscae_ug_kth_se/EkvNSq58i9VEh5Iqsg_l-UIB83xngGy-dJDav7ChcgHiJQ?e=J3qyYU) and put the assets directory in root.
2. Build asset_processor:
```bash
cd tools/asset_processor
cmake . -GNinja -Bbuild
cd build
ninja
cd ../../..
./tools/asset_processor/build/asset_processor manifest.json
```
3. Initial build:
```bash
cmake . -GNinja -Bbuild
cd build
ninja
cd ..
```
4. Build and Run
```bash
cd build; ninja; cd ..; ./build/game_engine
```
