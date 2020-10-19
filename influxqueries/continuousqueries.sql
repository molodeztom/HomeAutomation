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
