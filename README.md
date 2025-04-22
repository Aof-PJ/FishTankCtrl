# FishTankCtrl

### RTDB Structure

```
{
  "fishTank_ctrl": {
        <BOARD_ID>: {
            "LightCycle": {
                "closeHr": Int,
                "closeMin": Int,
                "maxHr": Int,
                "maxMin": Int,
                "startHr": Int,
                "startMin": Int
            },
            "ProcessNow": {
                "ChangeWaterNow": Boolean,
                "FeedNow": Boolean
            },
            "Sensor": {
                "FoodLvl": Int,
                "Temperature": Float
            },
            "Time": {
                "LastUpdate": {
                "Stamp": Time_Format_String,
                "UpdateFlag": Boolean
                },
                "Now": Time_Format_String
            },
            "WaterChanging": Int,
            "deviceName": String,
            "password": String
        }
    }
} 
```
#### Time format : yyyy-mm-ddTHH:MM-SS