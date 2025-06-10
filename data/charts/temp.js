//Code that creates a chart of realtime temperature data
var chartTempRT = new Highcharts.Chart({
    chart: { renderTo: 'chart-temperature-rt' },
    title: { text: 'Live Temperatures (Last 2 Minutes)' },
    series: [
        {
            type: "line",
            showInLegend: true,
            name: "Glycol",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Solar Preheat",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Room Ambient",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Cold Water",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Hot Water",
            data: []
        },
    ],
    plotOptions: {
        line: {
            animation: false,
            dataLabels: { enabled: true }
        },
    },
    xAxis: {
        title: { text: 'Time' },
        type: 'datetime',
        dateTimeLabelFormats: { second: '%H:%M:%S' }
    },
    yAxis: {
        title: { text: 'Temperature (Celsius)' }
    },
    credits: { enabled: false }
});
//Function that allows the chart to update every 6 seconds
setInterval(function () {
    let xhttp = new XMLHttpRequest(); //Create a data request
    xhttp.onreadystatechange = function () { //Callback function
        if (this.readyState == 4 && this.status == 200) { //When ready to receive
            let x = (new Date()).getTime(), //Current time
                y = this.responseText.split(',').map(Number); //Get the data as an array of floats
            for (let i = 0; i < 5; i++) {
                if (chartTempRT.series[i].data.length > 20) { //If there are more than 20 points
                    chartTempRT.series[i].addPoint([x, y[i]/100], true, true, true); //Add a point and shift
                } else {
                    chartTempRT.series[i].addPoint([x, y[i]/100], true, false, true); //Add a point
                }
            }
        }
    };
    xhttp.open("GET", "/temperature-rt", true); //Open the data request
    xhttp.send();   //Send the data request
}, 6000);  //Repeat every 6 seconds

// Chart of historical temperature data
var chartTempHistory = new Highcharts.stockChart({
    chart: {
        renderTo: 'chart-temperature-history',
        zooming: {
            mouseWheel: { enabled: false },
        },
    },
    title: { text: 'Historical Temperatures' },
    legend: { enabled: true },
    navigator: { enabled: false },
    scrollbar: { enabled: false },
    rangeSelector: {
        selected: 0, // Start with "All" selected by default (index 0)
        buttons: [
            { type: 'all', text: 'All' },
            { type: 'day', count: 1, text: '1d' },
            { type: 'hour', count: 1, text: '1h' },
            { type: 'minute', count: 1, text: '1m' }
        ],
        inputEnabled: true // Allows manual date typing
    },
    series: [
        {
            type: "line",
            showInLegend: true,
            name: "Glycol",
            color: "#1f77b4",
            dashStyle: "Dot",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Solar Preheat",
            color: "#ff7f0e",
            dashStyle: "Dash",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Room Ambient",
            color: "#2ca02c",
            dashStyle: "ShortDash",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Cold Water",
            color: "#17becf",
            dashStyle: "DashDot",
            data: []
        },
        {
            type: "line",
            showInLegend: true,
            name: "Hot Water",
            color: "#d62728",
            dashStyle: "Solid",
            data: []
        },
    ],
    plotOptions: {
        line: {
            dataLabels: { enabled: false },
            marker: { enabled: false }
        },
    },
    xAxis: {
        title: { text: 'Time' },
        type: 'datetime',
        events: {
            afterSetExtremes: function (e) { // To pass in the newly selected timestamp range
                const start = Math.floor(e.min / 1000); // Convert milliseconds to seconds (Highcharts uses milliseconds but we store timestamps as UNIX seconds)
                const end = Math.floor(e.max / 1000);
                fetch(`/temperature-range?start=${start}&end=${end}`)
                    .then(response => {
                        return response.json();
                    })
                    .then(data => {
                        chartTempHistory.series[0].setData(data.glycol);
                        chartTempHistory.series[1].setData(data.preheat);
                        chartTempHistory.series[2].setData(data.ambient);
                        chartTempHistory.series[3].setData(data.source);
                        chartTempHistory.series[4].setData(data.hot);
                    })
                    .catch(error => {
                        console.error("Error fetching historical data:", error);
                    });
            }
        }
    },
    yAxis: {
        title: { text: 'Temperature (Celsius)' },
        opposite: false,
    },
    credits: { enabled: false },
});

const fetchInitialData = () => {
    const now = Math.floor(Date.now() / 1000); // Unix seconds
    const start = now - (60 * 60 * 24); // Last 24 hours
    const end = now;

    fetch(`/temperature-range?start=${start}&end=${end}`)
        .then(response => {
            return response.json();
        })
        .then(data => {

            // Check if any of the series have data (avoid initializing until the CSV contains data)
            const hasData = data.glycol && data.glycol.length > 0;
            if (!hasData) {
                return;
            }

            chartTempHistory.series[0].setData(data.glycol);
            chartTempHistory.series[1].setData(data.preheat);
            chartTempHistory.series[2].setData(data.ambient);
            chartTempHistory.series[3].setData(data.source);
            chartTempHistory.series[4].setData(data.hot);
        })
        .catch(error => {
            console.error("Error fetching historical data:", error);
        });
};

fetchInitialData();
