Battery measurement:

	Messung alle 5 Minuten Speichern für 6 Monate > RP half_year 
	stündliche Werte Speichern für 1 Jahr ->RP one_year
	tägliche Werte Speichern für immer > RP forever
	
Wet anschauen:
SELECT mean("fVolt") FROM half_year.sSens1 WHERE time > now() - 20m GROUP BY time(1m)
	
	

RPs:
two_month
ALTER RETENTION POLICY "two_month" ON "HomeAutomation" DURATION 8w  
half_year
CREATE RETENTION POLICY "half_year" ON "HomeAutomation" DURATION 26w REPLICATION 1 
ALTER RETENTION POLICY "half_year" ON "HomeAutomation" DURATION 26w 
one_year
CREATE RETENTION POLICY "one_year" ON "HomeAutomation" DURATION 26w REPLICATION 1 
ALTER RETENTION POLICY "one_year" ON "HomeAutomation" DURATION 52w  
forever
CREATE RETENTION POLICY "forever" ON "HomeAutomation" DURATION INF REPLICATION 1 


DROP RETENTION POLICY two_hours ON HomeAutomation

CPs
Ventilstellungen
CREATE CONTINUOUS QUERY cq_5min_Tom_iSetTemp ON HomeAutomation BEGIN SELECT mean(iSetTemp) as iSetTemp INTO HomeAutomation.one_year.Tom FROM HomeAutomation.two_month.Tom GROUP BY time(5m)END


ValveControl
TempIn

CREATE CONTINUOUS QUERY cq_5min_VC_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) as TempIn INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m)END
CREATE CONTINUOUS QUERY cq_1hour_VC_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
TempOut
CREATE CONTINUOUS QUERY cq_5min_VC_TempOut ON HomeAutomation BEGIN SELECT mean(TempOut) as TempOut INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m)END
CREATE CONTINUOUS QUERY cq_1hour_VC_TempOut ON HomeAutomation BEGIN SELECT mean(TempOut) AS TempOut INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
ValveVolt
CREATE CONTINUOUS QUERY cq_5min_VC_ValveVolt ON HomeAutomation BEGIN SELECT mean(ValveVolt) as ValveVolt INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m)END


Sensor 1 Humi:
 CREATE CONTINUOUS QUERY cq_5min_S1_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
 CREATE CONTINUOUS QUERY cq_1hour_S1_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END


Sensor 5 fTempA:
CREATE CONTINUOUS QUERY cq_5min_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1hour_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END


CREATE CONTINUOUS QUERY cq_1_hour_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.half_year.sSens1 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1_day_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_1_hour_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.half_year.sSens2 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1_day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.one_year.sSens2 GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_1_hour_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.half_year.sSens3 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1_day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.one_year.sSens3 GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_1_hour_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.half_year.sSens5 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1_day_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1d) END

Löschen:
DROP CONTINUOUS QUERY  cq_1hour_ValveControl_TempIn ON HomeAutomation


---------------------------------------------------------------------------------------




CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens1 FROM HomeAutomation.two_hours.sSens1 GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_years.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1d) END

grafana query for measurement
SELECT distinct("fTempA") FROM two_hours.sSens1, two_month.sSens1, one_year.sSens1   ,two_years.sSens1 WHERE $timeFilter GROUP BY time($__interval) fill(linear)

SensorLOC: 

grafana query for measurement
SELECT distinct("fTempA") FROM two_hours.SensorLoc, two_month.SensorLoc, one_year.SensorLoc,two_years.SensorLoc WHERE $timeFilter GROUP BY time($__interval) fill(linear)

CREATE CONTINUOUS QUERY cq_5min_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_5min_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END

CREATE CONTINUOUS QUERY cq_5min_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1_hour_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1day_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END


 
CREATE CONTINUOUS QUERY "cq_5min" ON "HomeAutomation"
RESAMPLE EVERY 1h FOR 60m
BEGIN
  SELECT mean("passengers") INTO "average_passengers" FROM "bus_data" GROUP BY time(30m)
END
Weiterer Query für Maximum 
CREATE CONTINUOUS QUERY cq_5min_S1_fTempAMax ON HomeAutomation BEGIN SELECT max(fTempA) AS fTempAMax INTO HomeAutomation.two_month.sSens1 FROM HomeAutomation.two_hours.sSens1 GROUP BY time(5m) END
CREATE CONTINUOUS QUERY cq_1hour_S1_fTempAMin ON HomeAutomation BEGIN SELECT min(fTempA) AS fTempA, mod=MINM  INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END
und minimum

CREATE CONTINUOUS QUERY cq_1hour_S2_Min ON HomeAutomation BEGIN SELECT min(*) INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1h) END
CREATE CONTINUOUS QUERY cq_1hour_S2_Max ON HomeAutomation BEGIN SELECT max(*) INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1h) END

Aus den 5 min Werten Tages Max und Min für alle Werte in die Tagesauflösung schreiben
CREATE CONTINUOUS QUERY cq_one_day_SLoc_Max ON HomeAutomation BEGIN SELECT max(*) INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
CREATE CONTINUOUS QUERY cq_one_day_SLoc_Min ON HomeAutomation BEGIN SELECT min(*) INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END

Delete a single measurement:

delete from sSens3 where time >= '2020-04-26T12:45:00.000000000Z' AND time <= '2020-04-26T12:55:00Z'


28.02.2021 continuos queries from raspberry 2
name: HomeAutomation
name                  query
----                  -----
cq_5min_Humi          CREATE CONTINUOUS QUERY cq_5min_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.sSens1 FROM HomeAutomation.two_hours.sSens1 GROUP BY time(5m) END
cq_1_hour_Humi        CREATE CONTINUOUS QUERY cq_1_hour_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END
cq_1day_Humi          CREATE CONTINUOUS QUERY cq_1day_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_years.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1d) END
cq_5min_S1_ERR        CREATE CONTINUOUS QUERY cq_5min_S1_ERR ON HomeAutomation BEGIN SELECT mean(ERR) AS ERR INTO HomeAutomation.two_month.sSens1 FROM HomeAutomation.two_hours.sSens1 GROUP BY time(5m) END
cq_1_hour_S1_ERR      CREATE CONTINUOUS QUERY cq_1_hour_S1_ERR ON HomeAutomation BEGIN SELECT mean(ERR) AS ERR INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END
cq_1day_S1_ERR        CREATE CONTINUOUS QUERY cq_1day_S1_ERR ON HomeAutomation BEGIN SELECT mean(ERR) AS ERR INTO HomeAutomation.two_years.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1d) END
cq_5min_S2_fTempA     CREATE CONTINUOUS QUERY cq_5min_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens2 FROM HomeAutomation.two_hours.sSens2 GROUP BY time(5m) END
cq_1_hour_S2_fTempA   CREATE CONTINUOUS QUERY cq_1_hour_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1h) END
cq_1day_S2_fTempA     CREATE CONTINUOUS QUERY cq_1day_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_years.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1d) END
cq_5min_S2_fVolt      CREATE CONTINUOUS QUERY cq_5min_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.two_month.sSens2 FROM HomeAutomation.two_hours.sSens2 GROUP BY time(5m) END
cq_1_hour_S2_fVolt    CREATE CONTINUOUS QUERY cq_1_hour_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1h) END
cq_1day_S2_fVolt      CREATE CONTINUOUS QUERY cq_1day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.two_years.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1d) END
cq_5min_S2_Err        CREATE CONTINUOUS QUERY cq_5min_S2_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_month.sSens2 FROM HomeAutomation.two_hours.sSens2 GROUP BY time(5m) END
cq_1_hour_S2_Err      CREATE CONTINUOUS QUERY cq_1_hour_S2_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1h) END
cq_1day_S2_Err        CREATE CONTINUOUS QUERY cq_1day_S2_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_years.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(1d) END
cq_5min_S3_Err        CREATE CONTINUOUS QUERY cq_5min_S3_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_month.sSens3 FROM HomeAutomation.two_hours.sSens3 GROUP BY time(5m) END
cq_1_hour_S3_Err      CREATE CONTINUOUS QUERY cq_1_hour_S3_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1h) END
cq_1day_S3_Err        CREATE CONTINUOUS QUERY cq_1day_S3_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_years.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1d) END
cq_5min_S3_fTempA     CREATE CONTINUOUS QUERY cq_5min_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens3 FROM HomeAutomation.two_hours.sSens3 GROUP BY time(5m) END
cq_1_hour_S3_fTempA   CREATE CONTINUOUS QUERY cq_1_hour_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1h) END
cq_1day_S3_fTempA     CREATE CONTINUOUS QUERY cq_1day_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_years.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1d) END
cq_5min_S3_fVolt      CREATE CONTINUOUS QUERY cq_5min_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.two_month.sSens3 FROM HomeAutomation.two_hours.sSens3 GROUP BY time(5m) END
cq_1_hour_S3_fVolt    CREATE CONTINUOUS QUERY cq_1_hour_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1h) END
cq_1day_S3_fVolt      CREATE CONTINUOUS QUERY cq_1day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.two_years.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(1d) END
cq_1day_SLoc_fVolt    CREATE CONTINUOUS QUERY cq_1day_SLoc_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
cq_5min_SLoc_fTempA   CREATE CONTINUOUS QUERY cq_5min_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_fTempA CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
cq_1day_SLoc_fTempA   CREATE CONTINUOUS QUERY cq_1day_SLoc_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
cq_5min_SLoc_fHumi    CREATE CONTINUOUS QUERY cq_5min_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_fHumi  CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
cq_1day_SLoc_fHumi    CREATE CONTINUOUS QUERY cq_1day_SLoc_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
cq_5min_SLoc_iLight   CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_iLight CREATE CONTINUOUS QUERY cq_1_hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
cq_1day_SLoc_iLight   CREATE CONTINUOUS QUERY cq_1day_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
cq_5min_SLoc_fAtmo    CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_fAtmo  CREATE CONTINUOUS QUERY cq_1_hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
cq_1day_SLoc_fAtmo    CREATE CONTINUOUS QUERY cq_1day_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END
cq_5min_SLoc_Err      CREATE CONTINUOUS QUERY cq_5min_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_month.SensorLoc FROM HomeAutomation.two_hours.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_Err    CREATE CONTINUOUS QUERY cq_1_hour_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1h) END
cq_1day_SLoc_Err      CREATE CONTINUOUS QUERY cq_1day_SLoc_Err ON HomeAutomation BEGIN SELECT mean(Err) AS Err INTO HomeAutomation.two_years.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(1d) END


neue queries 01.03.2021

----------Sensor 1 ------------------
cq_5min_S1_fTempA     CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
cq_1_hour_S1_fTempA  CREATE CONTINUOUS QUERY cq_1hour_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_1_hour_LG_S1_fTempA  CREATE CONTINUOUS QUERY cq_1_hour_LG_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.two_years.sSens1 GROUP BY time(1h) END

cq_1hour_S1_fTempAMax CREATE CONTINUOUS QUERY cq_1hour_S1_fTempAMax ON HomeAutomation BEGIN SELECT max(fTempA) AS fTempAMax INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END
cq_1hour_S1_fTempAMin CREATE CONTINUOUS QUERY cq_1hour_S1_fTempAMin ON HomeAutomation BEGIN SELECT min(fTempA) AS fTempAMin INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(1h) END

----------Sensor 2 ------------------
cq_5min_S2_fTempA     CREATE CONTINUOUS QUERY cq_5min_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(5m) END
cq_1_hour_S2_fTempA  CREATE CONTINUOUS QUERY cq_1hour_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.one_year.sSens2 GROUP BY time(1h) END
cq_1_hour_LG_S2_fTempA  CREATE CONTINUOUS QUERY cq_1_hour_LG_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.two_years.sSens2 GROUP BY time(1h) END
cq_1_day_S2_fVolt    CREATE CONTINUOUS QUERY cq_1_day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.half_year.sSens2 GROUP BY time(1d) END

----------Sensor 3 ------------------
cq_5min_S3_fTempA     CREATE CONTINUOUS QUERY cq_5min_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(5m) END
cq_1_hour_S3_fTempA  CREATE CONTINUOUS QUERY cq_1hour_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.one_year.sSens3 GROUP BY time(1h) END
cq_1_hour_LG_S3_fTempA  CREATE CONTINUOUS QUERY cq_1_hour_LG_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.two_years.sSens3 GROUP BY time(1h) END
cq_1_day_S3_fVolt    CREATE CONTINUOUS QUERY cq_1_day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.half_year.sSens3 GROUP BY time(1d) END

----------Sensor 5 ------------------
cq_5min_LG_S5_fTempA     CREATE CONTINUOUS QUERY cq_5min_LG_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fTempA     CREATE CONTINUOUS QUERY cq_5min_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1_hour_S5_fTempA  CREATE CONTINUOUS QUERY cq_1hour_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_5min_LG_S5_fHumi     CREATE CONTINUOUS QUERY cq_5min_LG_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fHumi     CREATE CONTINUOUS QUERY cq_5min_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1_hour_S5_fHumi  CREATE CONTINUOUS QUERY cq_1hour_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_1_day_S5_fVolt    CREATE CONTINUOUS QUERY cq_1_day_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.half_year.sSens5 GROUP BY time(1d) END

----------Sensor LOC ------------------

cq_5min_SLoc_iLight     CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(5m) END
cq_1_hour_SLoc_iLight  CREATE CONTINUOUS QUERY cq_1hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(1h) END

cq_5min_SLoc_fAtmo     CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(30m) END
cq_1_hour_SLoc_fAtmo  CREATE CONTINUOUS QUERY cq_1hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(2h) END


----------ValveControl 1 ------------------
cq_5min_ValveControl_TempOut    CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1_hour_ValveControl_TempOut CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END

cq_5min_ValveControl_TempIn    CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1_hour_ValveControl_TempIn CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END

Aus DB stand 02.03.2021 21:16

name: HomeAutomation
name                          query
----                          -----
cq_1_hour_S1_fVolt            CREATE CONTINUOUS QUERY cq_1_hour_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.half_year.sSens1 GROUP BY time(1h) END
cq_1_day_S1_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1d) END
cq_5min_Humi                  CREATE CONTINUOUS QUERY cq_5min_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.sSens1 FROM HomeAutomation.two_hours.sSens1 GROUP BY time(5m) END
cq_5min_S1_fTempA             CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
cq_1hour_S1_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_1_hour_LG_S1_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.two_years.sSens1 GROUP BY time(1h) END
cq_5min_S2_fTempA             CREATE CONTINUOUS QUERY cq_5min_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(5m) END
cq_1hour_S2_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.one_year.sSens2 GROUP BY time(1h) END
cq_1_hour_LG_S2_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.two_years.sSens2 GROUP BY time(1h) END
cq_1_day_S2_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.half_year.sSens2 GROUP BY time(1d) END
cq_5min_S3_fTempA             CREATE CONTINUOUS QUERY cq_5min_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(5m) END
cq_1hour_S3_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.one_year.sSens3 GROUP BY time(1h) END
cq_1_hour_LG_S3_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.two_years.sSens3 GROUP BY time(1h) END
cq_1_day_S3_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.half_year.sSens3 GROUP BY time(1d) END
cq_5min_LG_S5_fTempA          CREATE CONTINUOUS QUERY cq_5min_LG_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fTempA             CREATE CONTINUOUS QUERY cq_5min_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_5min_LG_S5_fHumi           CREATE CONTINUOUS QUERY cq_5min_LG_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fHumi              CREATE CONTINUOUS QUERY cq_5min_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fHumi             CREATE CONTINUOUS QUERY cq_1hour_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_1_day_S5_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.half_year.sSens5 GROUP BY time(1d) END
cq_5min_SLoc_iLight           CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(5m) END
cq_1hour_SLoc_iLight          CREATE CONTINUOUS QUERY cq_1hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(1h) END
cq_5min_SLoc_fAtmo            CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(30m) END
cq_1hour_SLoc_fAtmo           CREATE CONTINUOUS QUERY cq_1hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(2h) END
cq_5min_ValveControl_TempOut  CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempOut CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
cq_5min_ValveControl_TempIn   CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempIn  CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END


Manuell einmalig kopieren geht so:
SELECT fTempA INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 Where time >now() - 10w and time < now()  möglichst immer 10 Wochen intervalle
Aus DB Stand 03.03.21 23:17

name      duration   shardGroupDuration replicaN default
----      --------   ------------------ -------- -------
autogen   0s         168h0m0s           1        true
two_month 1344h0m0s  1h0m0s             1        false
two_years 17472h0m0s 24h0m0s            1        false
one_year  8736h0m0s  1h0m0s             1        false
half_year 4368h0m0s  168h0m0s           1        false
forever   0s         168h0m0s           1        false

name: HomeAutomation
name                          query
----                          -----
cq_1_hour_S1_fVolt            CREATE CONTINUOUS QUERY cq_1_hour_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.half_year.sSens1 GROUP BY time(1h) END
cq_1_day_S1_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1d) END
cq_5min_Humi                  CREATE CONTINUOUS QUERY cq_5min_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
cq_1hour_fHumi                CREATE CONTINUOUS QUERY cq_1hour_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_5min_S1_fTempA             CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
cq_1hour_S1_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_1_hour_LG_S1_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.two_years.sSens1 GROUP BY time(1h) END
cq_5min_S2_fTempA             CREATE CONTINUOUS QUERY cq_5min_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(5m) END
cq_1hour_S2_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.one_year.sSens2 GROUP BY time(1h) END
cq_1_hour_LG_S2_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.two_years.sSens2 GROUP BY time(1h) END
cq_1_day_S2_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.half_year.sSens2 GROUP BY time(1d) END
cq_5min_S3_fTempA             CREATE CONTINUOUS QUERY cq_5min_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(5m) END
cq_1hour_S3_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.one_year.sSens3 GROUP BY time(1h) END
cq_1_hour_LG_S3_fTempA        CREATE CONTINUOUS QUERY cq_1_hour_LG_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.two_years.sSens3 GROUP BY time(1h) END
cq_1_day_S3_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.half_year.sSens3 GROUP BY time(1d) END
cq_5min_LG_S5_fTempA          CREATE CONTINUOUS QUERY cq_5min_LG_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fTempA             CREATE CONTINUOUS QUERY cq_5min_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_5min_LG_S5_fHumi           CREATE CONTINUOUS QUERY cq_5min_LG_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.two_month.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END
cq_5min_S5_fHumi              CREATE CONTINUOUS QUERY cq_5min_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fHumi             CREATE CONTINUOUS QUERY cq_1hour_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_1_day_S5_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.half_year.sSens5 GROUP BY time(1d) END
cq_5min_SLoc_iLight           CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(5m) END
cq_1hour_SLoc_iLight          CREATE CONTINUOUS QUERY cq_1hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(1h) END
cq_5min_SLoc_fAtmo            CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(30m) END
cq_1hour_SLoc_fAtmo           CREATE CONTINUOUS QUERY cq_1hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(2h) END
cq_5min_ValveControl_TempOut  CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempOut CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
cq_5min_ValveControl_TempIn   CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempIn  CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
cq_S5_Test                    CREATE CONTINUOUS QUERY cq_S5_Test ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempATest INTO HomeAutomation.autogen.sSens5 FROM HomeAutomation.autogen.sSens5 GROUP BY time(5m) END


DROP RETENTION POLICY two_month ON HomeAutomation
DROP RETENTION POLICY half_year ON HomeAutomation
DROP RETENTION POLICY one_year ON HomeAutomation

DROP CONTINUOUS QUERY  cq_1hour_S1_fTempA   ON HomeAutomation
DROP CONTINUOUS QUERY  cq_1hour_Humi  ON HomeAutomation
DROP CONTINUOUS QUERY  cq_1hour_ValveControl_TempOut  ON HomeAutomation

CQ mit rundung auf 2 Stellen (gespeichert wird trotzdem ein float. 
CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT round(mean(fTempA)*100))/100 AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END

Stand      01.06.21

> show continuous queries
name: TEST
name query
---- -----

name: HomeAutomation
name                          query
----                          -----
cq_1_hour_S1_fVolt            CREATE CONTINUOUS QUERY cq_1_hour_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.half_year.sSens1 GROUP BY time(1h) END
cq_1_day_S1_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S1_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1d) END
cq_5min_S2_fTempA             CREATE CONTINUOUS QUERY cq_5min_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens2 FROM HomeAutomation.two_month.sSens2 GROUP BY time(5m) END
cq_1hour_S2_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S2_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.one_year.sSens2 GROUP BY time(1h) END
cq_1_day_S2_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S2_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens2 FROM HomeAutomation.half_year.sSens2 GROUP BY time(1d) END
cq_5min_S3_fTempA             CREATE CONTINUOUS QUERY cq_5min_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens3 FROM HomeAutomation.two_month.sSens3 GROUP BY time(5m) END
cq_1hour_S3_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S3_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.one_year.sSens3 GROUP BY time(1h) END
cq_1_day_S3_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S3_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens3 FROM HomeAutomation.half_year.sSens3 GROUP BY time(1d) END
cq_5min_S5_fHumi              CREATE CONTINUOUS QUERY cq_5min_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fHumi             CREATE CONTINUOUS QUERY cq_1hour_S5_fHumi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_1_day_S5_fVolt             CREATE CONTINUOUS QUERY cq_1_day_S5_fVolt ON HomeAutomation BEGIN SELECT mean(fVolt) AS fVolt INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.half_year.sSens5 GROUP BY time(1d) END
cq_5min_SLoc_iLight           CREATE CONTINUOUS QUERY cq_5min_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(5m) END
cq_1hour_SLoc_iLight          CREATE CONTINUOUS QUERY cq_1hour_SLoc_iLight ON HomeAutomation BEGIN SELECT mean(iLight) AS iLight INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(1h) END
cq_5min_SLoc_fAtmo            CREATE CONTINUOUS QUERY cq_5min_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.one_year.SensorLoc FROM HomeAutomation.two_month.SensorLoc GROUP BY time(30m) END
cq_1hour_SLoc_fAtmo           CREATE CONTINUOUS QUERY cq_1hour_SLoc_fAtmo ON HomeAutomation BEGIN SELECT mean(fAtmo) AS fAtmo INTO HomeAutomation.forever.SensorLoc FROM HomeAutomation.one_year.SensorLoc GROUP BY time(2h) END
cq_5min_ValveControl_TempOut  CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempOut CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempOut ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempOut INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
cq_5min_ValveControl_TempIn   CREATE CONTINUOUS QUERY cq_5min_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.one_year.ValveControl FROM HomeAutomation.two_month.ValveControl GROUP BY time(5m) END
cq_1hour_ValveControl_TempIn  CREATE CONTINUOUS QUERY cq_1hour_ValveControl_TempIn ON HomeAutomation BEGIN SELECT mean(TempIn) AS TempIn INTO HomeAutomation.forever.ValveControl FROM HomeAutomation.one_year.ValveControl GROUP BY time(1h) END
cq_5min_S1_fTempA             CREATE CONTINUOUS QUERY cq_5min_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END
cq_1hour_S1_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S1_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_5min_S5_fTempA             CREATE CONTINUOUS QUERY cq_5min_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.one_year.sSens5 FROM HomeAutomation.two_month.sSens5 GROUP BY time(5m) END
cq_1hour_S5_fTempA            CREATE CONTINUOUS QUERY cq_1hour_S5_fTempA ON HomeAutomation BEGIN SELECT mean(fTempA) AS fTempA INTO HomeAutomation.forever.sSens5 FROM HomeAutomation.one_year.sSens5 GROUP BY time(1h) END
cq_1hour_Humi                 CREATE CONTINUOUS QUERY cq_1hour_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.forever.sSens1 FROM HomeAutomation.one_year.sSens1 GROUP BY time(1h) END
cq_5min_Humi                  CREATE CONTINUOUS QUERY cq_5min_Humi ON HomeAutomation BEGIN SELECT mean(fHumi) AS fHumi INTO HomeAutomation.one_year.sSens1 FROM HomeAutomation.two_month.sSens1 GROUP BY time(5m) END



